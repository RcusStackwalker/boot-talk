#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#include <stdint.h>
#include <QTimer>
#include "J2534.h"

J2534 j2534;
unsigned long devId = 0xffff;
unsigned long channelId = 0xffff;

void init_adapter()
{
    j2534.init();
    //j2534.debug(true);
    j2534.PassThruOpen(NULL, &devId);
    j2534.PassThruConnect(devId, ISO9141, ISO9141_NO_CHECKSUM, 9600, &channelId);

    SCONFIG_LIST scl;
    SCONFIG scp[] = {{P1_MAX,0},{PARITY,0}, {LOOPBACK, 1}, {P4_MIN, 0}, {P3_MIN, 0} };
    scl.NumOfParams = sizeof(scp) / sizeof(scp[0]);
    scl.ConfigPtr = scp;
    j2534.PassThruIoctl(channelId,SET_CONFIG,&scl,NULL);

    PASSTHRU_MSG rxmsg,txmsg;
    PASSTHRU_MSG msgMask,msgPattern;
    unsigned long msgId;
    unsigned long numRxMsg;

    // simply create a "pass all" filter so that we can see
    // everything unfiltered in the raw stream

    txmsg.ProtocolID = ISO9141;
    txmsg.RxStatus = 0;
    txmsg.TxFlags = 0;
    txmsg.Timestamp = 0;
    txmsg.DataSize = 1;
    txmsg.ExtraDataIndex = 0;
    msgMask = msgPattern  = txmsg;
    memset(msgMask.Data,0,1); // mask the first 4 byte to 0
    memset(msgPattern.Data,0,1);// match it with 0 (i.e. pass everything)
    j2534.PassThruStartMsgFilter(channelId,PASS_FILTER,&msgMask,&msgPattern,NULL,&msgId);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    init_adapter();
    QTimer *t = new QTimer(this);
    t->setInterval(10);
    connect(t, SIGNAL(timeout()), this, SLOT(onReadTimerTimeout()));
    t->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString bytesToString(const uint8_t *data, unsigned size)
{
    QString ret;
    for (unsigned i = 0; i < size; ++i) {
        if (i > 0)
            ret.append(' ');
        ret.append(QString("%1").arg(data[i], 2, 16, QLatin1Char('0')));
    }
    return ret;
}

void stringToBytes(const QString &str, uint8_t *data, unsigned long *size)
{
    QStringList list = str.split(' ');
    for (int i = 0; i < list.size(); ++i) {
        data[i] = list.at(i).toUInt(0, 16);
    }
    *size = list.size();
}

void MainWindow::onReadTimerTimeout()
{
    unsigned long msgCount = 1;
    PASSTHRU_MSG msg;
    QString ret;
    do {
        j2534.PassThruReadMsgs(channelId, &msg, &msgCount, 0);
        if (msgCount && msg.DataSize) {
            if (ret.size())
                ret.append(' ');
            ret.append(bytesToString(msg.Data, msg.DataSize));
        }
    } while (msgCount);
    if (ret.size()) {
        ui->textEdit->append(QString::fromLatin1("< %1").arg(ret));
        ui->textEdit->scroll(0,1);
    }

}

void MainWindow::on_pushButton_2_clicked()
{
    qDebug("Send clicked");

    uint8_t data[128];
    unsigned long size;
    stringToBytes(ui->lineEdit->text(), data, &size);
    adapterWrite(data, size);
    ui->lineEdit->clear();
}

void MainWindow::on_rebootButton_clicked()
{
    /*set boot voltage*/
    j2534.PassThruSetProgrammingVoltage(devId, AUX_PIN, VOLTAGE_OFF);
    QThread::msleep(500);
    j2534.PassThruSetProgrammingVoltage(devId, AUX_PIN, 17000);
    QThread::msleep(500);

}

uint8_t init10_code[] = { 0x10 };
uint8_t init_code_body[] = { 0x00, 0x03 };
//uint8_t init_code_body[] = { 0xff, 0xff };
uint8_t after_init_code[] = { 0x00 };
//uint8_t password_body[] = { 0x9b, 0xec, 0x2b, 0x8b, 0xd4, 0x86, 0x05 };
uint8_t password_body[] = { 0x35, 0x37, 0x38, 0x37, 0x34, 0x36, 0x05 };
//uint8_t password_body[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05 };
uint8_t final_entry_code[] = { 0xc1 };
uint8_t preerase1_body[] = { 0x20 };
uint8_t preerase2_body[] = { 0x40 };
uint8_t erase_sequence[] = { 0x3a, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0xfc };
//3a 02 00 00 02 00 00 fc

void MainWindow::on_init1Button_clicked()
{
    on_rebootButton_clicked();
    setDataRate(9600);
    adapterWriteAndWait(init10_code, sizeof(init10_code));
    adapterWriteAndWait(init_code_body, sizeof(init_code_body));
    adapterWriteAndWait(after_init_code, sizeof(after_init_code));

    for (int i = 0; i < sizeof(password_body); ++i) {
        adapterWriteAndWait(&password_body[i], 1, 200);
    }

    setDataRate(62500);
    adapterWriteAndWait(final_entry_code, sizeof(final_entry_code));

    adapterWriteAndWait(preerase1_body, sizeof(preerase1_body));
    adapterWriteAndWait(preerase2_body, sizeof(preerase2_body));
    adapterWriteAndWait(erase_sequence, sizeof(erase_sequence));
}

void MainWindow::adapterWrite(const uint8_t *data, unsigned size)
{
    unsigned long msgCount = 1;
    PASSTHRU_MSG msg;
    memset(&msg, 0, sizeof(msg));
    memcpy(msg.Data, data, size);
    msg.DataSize = size;
    msg.ProtocolID = ISO9141;
    j2534.PassThruWriteMsgs(channelId, &msg, &msgCount, 10);

    ui->textEdit->append(QString::fromLatin1("> %1\n").arg(bytesToString(data, size)));
    ui->textEdit->scroll(0, 1);

}

void MainWindow::adapterWriteAndWait(const uint8_t *data, unsigned size, unsigned timeout)
{
    adapterWrite(data, size);
    QThread::msleep(timeout);
    QApplication::processEvents();
}

void MainWindow::setDataRate(unsigned dr)
{
    SCONFIG_LIST scl;
    SCONFIG scp[] = {{DATA_RATE, dr}};
    scl.NumOfParams = sizeof(scp) / sizeof(scp[0]);
    scl.ConfigPtr = scp;
    j2534.PassThruIoctl(channelId,SET_CONFIG,&scl,NULL);
}
