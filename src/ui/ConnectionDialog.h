#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>

namespace Witra {

class ConnectionDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ConnectionDialog(const QString& senderName, const QString& verificationCode,
                             QWidget* parent = nullptr);
    
    QString verificationCode() const { return m_verificationCode; }
    
    // Removed shadowed signals: accepted(), rejected()
    
private:
    void setupUi();
    void applyStyles();
    
    QString m_senderName;
    QString m_verificationCode;
};

} // namespace Witra

#endif // CONNECTIONDIALOG_H
