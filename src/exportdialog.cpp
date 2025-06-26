#include "exportdialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>

ExportDialog::ExportDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("导出设置");
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel("选择截至时间:"));
    endTimeEdit = new QDateTimeEdit(QDate::currentDate(), this);
    endTimeEdit->setCalendarPopup(true);
    layout->addWidget(endTimeEdit);

    layout->addWidget(new QLabel("设置播放次数（次）:"));
    playCountsEdit = new QLineEdit(this);
    playCountsEdit->setPlaceholderText("请输入播放次数（次）");
    layout->addWidget(playCountsEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    okButton = new QPushButton("确定", this);
    cancelButton = new QPushButton("取消", this);
    btnLayout->addWidget(okButton);
    btnLayout->addWidget(cancelButton);
    layout->addLayout(btnLayout);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

QDateTime ExportDialog::getEndTime() const
{
    return endTimeEdit->dateTime();
}

QString ExportDialog::getPlayCounts() const
{
    return playCountsEdit->text();
} 
