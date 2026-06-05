#include "StreamDecoder.h"

#include <algorithm>
#include <atomic>

uint64_t StreamDecoder::ptsToMs(int64_t pts, int64_t dts)
{
    const int64_t ts = (pts > 0) ? pts : dts;
    if (ts <= 0) {
        return 0;
    }
    return static_cast<uint64_t>(ts * 1000 / 90000);
}

bool StreamDecoder::hasTimestampIndex() const
{
    return !_timestampIndex.empty() && _durationMs > 0;
}

void StreamDecoder::finalizeTimestampIndex()
{
    if (_timestampIndex.empty()) {
        _startPtsMs = 0;
        _durationMs = 0;
        return;
    }

    std::sort(_timestampIndex.begin(), _timestampIndex.end(),
              [](const TimestampIndexEntry& a, const TimestampIndexEntry& b) {
                  return a.ptsMs < b.ptsMs;
              });

    _timestampIndex.erase(
        std::unique(_timestampIndex.begin(), _timestampIndex.end(),
                    [](const TimestampIndexEntry& a, const TimestampIndexEntry& b) {
                        return a.offset == b.offset;
                    }),
        _timestampIndex.end());

    _startPtsMs = _timestampIndex.front().ptsMs;
    const uint64_t endPtsMs = _timestampIndex.back().ptsMs;
    _durationMs = (endPtsMs > _startPtsMs) ? (endPtsMs - _startPtsMs) : 1;

    _frameNumMap.clear();
    _biFrameNumMap.clear();
    for (size_t i = 0; i < _timestampIndex.size(); ++i) {
        _frameNumMap.emplace(_timestampIndex[i].offset, i);
        _biFrameNumMap.emplace(i, _timestampIndex[i].offset);
    }
}

qint64 StreamDecoder::getProgressMs(uint64_t ptsMs, size_t keyFrameNum, size_t byteOffset) const
{
    if (hasTimestampIndex() && ptsMs >= _startPtsMs) {
        return static_cast<qint64>(ptsMs - _startPtsMs);
    }

    if (_fileSize > 0 && byteOffset > 0 && _durationMs > 0) {
        return static_cast<qint64>(byteOffset) * static_cast<qint64>(_durationMs)
               / static_cast<qint64>(_fileSize);
    }

    const int percent = getProgressPercent(ptsMs, keyFrameNum, byteOffset);
    if (_durationMs > 0) {
        return static_cast<qint64>(percent) * static_cast<qint64>(_durationMs) / 100;
    }
    return 0;
}

int StreamDecoder::getProgressPercent(uint64_t ptsMs, size_t keyFrameNum, size_t byteOffset) const
{
    if (hasTimestampIndex() && ptsMs > _startPtsMs) {
        const uint64_t elapsed = ptsMs - _startPtsMs;
        const int percent = static_cast<int>(elapsed * 100 / _durationMs);
        if (percent < 0) {
            return 0;
        }
        if (percent > 100) {
            return 100;
        }
        return percent;
    }

    if (_fileSize > 0 && byteOffset > 0) {
        const int percent = static_cast<int>(byteOffset * 100 / _fileSize);
        if (percent < 0) {
            return 0;
        }
        if (percent > 99) {
            return 99;
        }
        return percent;
    }

    if (keyFrameNum == 0 && byteOffset > 0 && !_frameNumMap.empty()) {
        keyFrameNum = resolveKeyFrameIndex(byteOffset);
    }

    const size_t total = getKeyFrameSum();
    if (total == 0) {
        return 0;
    }
    if (total == 1) {
        return keyFrameNum > 0 ? 100 : 0;
    }
    const int percent = static_cast<int>(keyFrameNum * 100 / (total - 1));
    if (percent < 0) {
        return 0;
    }
    if (percent > 100) {
        return 100;
    }
    return percent;
}

size_t StreamDecoder::resolveKeyFrameIndex(size_t byteOffset) const
{
    if (_frameNumMap.empty()) {
        return 0;
    }

    auto it = _frameNumMap.upper_bound(byteOffset);
    if (it == _frameNumMap.begin()) {
        return 0;
    }
    --it;
    return it->second;
}

size_t StreamDecoder::getOffsetForTimePercent(int percent) const
{
    if (_timestampIndex.empty()) {
        return getOffsetForPosition(percent);
    }

    if (percent <= 0) {
        return _timestampIndex.front().offset;
    }
    if (percent >= 100) {
        return _timestampIndex.back().offset;
    }

    const uint64_t targetMs = _startPtsMs + _durationMs * static_cast<uint64_t>(percent) / 100;

    size_t lo = 0;
    size_t hi = _timestampIndex.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (_timestampIndex[mid].ptsMs <= targetMs) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (lo == 0) {
        return _timestampIndex.front().offset;
    }
    return _timestampIndex[lo - 1].offset;
}

size_t StreamDecoder::resolveSeekOffset(int percent) const
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }

    if (hasTimestampIndex()) {
        return getOffsetForTimePercent(percent);
    }
    if (!_frameNumMap.empty()) {
        return getOffsetForPosition(percent);
    }
    if (_fileSize > 0) {
        return _fileSize * static_cast<size_t>(percent) / 100;
    }
    return 0;
}

void StreamDecoder::requestSeek(int percent)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    _pendingSeekPercent.store(percent);
    _needSeek.store(true);
    _seekReadBudget.store(32);
}

bool StreamDecoder::consumeSeekRequest(size_t& outOffset)
{
    if (!_needSeek.exchange(false)) {
        return false;
    }

    const int pos = _pendingSeekPercent.load();
    if (_fileSize == 0 && !hasTimestampIndex() && _frameNumMap.empty()) {
        return false;
    }

    outOffset = resolveSeekOffset(pos);
    _seekReadBudget.store(32);
    return true;
}
