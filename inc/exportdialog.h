#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QPushButton>

class ExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(QWidget *parent = nullptr);

    QDateTime getEndTime() const;
    QString getPlayCounts() const;

private:
    QDateTimeEdit *endTimeEdit;
    QLineEdit *playCountsEdit;
    QPushButton *okButton;
    QPushButton *cancelButton;
};

#endif // EXPORTDIALOG_H 
