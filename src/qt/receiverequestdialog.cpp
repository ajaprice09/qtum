// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/receiverequestdialog.h>
#include <qt/forms/ui_receiverequestdialog.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/qrimagewidget.h>
#include <qt/walletmodel.h>
#include <qt/styleSheet.h>
#include <qt/platformstyle.h>
#include <qt/addresstablemodel.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/receivecoinsdialog.h>

#include <QDialog>
#include <QString>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h> /* for USE_QRCODE */
#endif

ReceiveRequestDialog::ReceiveRequestDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent, GUIUtil::dialog_flags),
    ui(new Ui::ReceiveRequestDialog),
    model(nullptr),
    platformStyle(_platformStyle),
    requestPaymentDialog(0)
{
    ui->setupUi(this);
    GUIUtil::handleCloseWindowShortcut(this);
    requestPaymentDialog = new ReceiveCoinsDialog(platformStyle, this);

    SetObjectStyleSheet(ui->btnRefreshAddress, StyleSheetNames::ButtonLight);
    ui->btnCopyAddress->setIcon(platformStyle->MultiStatesIcon(":/icons/editcopy", PlatformStyle::PushButtonIcon));
    ui->btnCopyURI->setIcon(platformStyle->MultiStatesIcon(":/icons/editcopy", PlatformStyle::PushButtonIcon));

#ifndef USE_QRCODE
    ui->widgetQRMargin->setVisible(false);
#endif
}

ReceiveRequestDialog::~ReceiveRequestDialog()
{
    delete ui;
}

void ReceiveRequestDialog::setModel(WalletModel *_model)
{
    this->model = _model;
    if (_model && _model->getOptionsModel())
    {
        connect(_model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &ReceiveRequestDialog::update);

        // Set the button to be enabled or disabled based on whether the wallet can give out new addresses.
        ui->btnRefreshAddress->setEnabled(model->wallet().canGetAddresses());

        // Enable/disable the receive button if the wallet is now able/unable to give out new addresses.
        connect(model, &WalletModel::canGetAddressesChanged, [this] {
            bool canGetAddresses = model->wallet().canGetAddresses();
            bool getDefault = canGetAddresses && !ui->btnRefreshAddress->isEnabled() && ui->address_content->text().isEmpty();
            ui->btnRefreshAddress->setEnabled(canGetAddresses);
            if(getDefault && getDefaultAddress())
            {
                update();
            }
        });
    }

    requestPaymentDialog->setModel(model);


    // update the display unit if necessary
    update();
}

void ReceiveRequestDialog::setInfo(const SendCoinsRecipient &_info)
{
    this->info = _info;
    update();
}

bool ReceiveRequestDialog::refreshAddress()
{
    if(!model || !model->getAddressTableModel() || !model->getRecentRequestsTableModel())
        return false;

    /* Generate new receiving address */
    OutputType address_type = model->wallet().getDefaultAddressType();
    info.address = model->getAddressTableModel()->addRow(AddressTableModel::Receive, info.label, "", address_type);

    /* Store request for later reference */
    if(!info.address.isEmpty())
        model->getRecentRequestsTableModel()->addNewRequest(info);

    return true;
}

bool ReceiveRequestDialog::getDefaultAddress()
{
    if(!model || !model->getRecentRequestsTableModel())
        return false;

    // Get the last address from the request history list that have empty label, message and amount
    const RecentRequestsTableModel *submodel = model->getRecentRequestsTableModel();
    bool foundDefault = false;
    for(int i = submodel->rowCount(QModelIndex()) -1; i >= 0; i--)
    {
        SendCoinsRecipient entry = submodel->entry(i).recipient;
        if(entry.label.isEmpty() && entry.message.isEmpty() && entry.amount == 0)
        {
            info = entry;
            foundDefault = true;
            break;
        }
    }

    // Generate new address if no default found
    if(!foundDefault)
    {
        info = SendCoinsRecipient();
        refreshAddress();
    }
    
    return !info.address.isEmpty();
}

void ReceiveRequestDialog::update()
{
    setWindowTitle(tr("Request payment to %1").arg(info.label.isEmpty() ? info.address : info.label));
    QString uri = GUIUtil::formatBitcoinURI(info);

    if(!info.address.isEmpty())
    {
        QString uri = GUIUtil::formatBitcoinURI(info);
#ifdef USE_QRCODE
        if(ui->qr_code->setQR(uri))
        {
            ui->qr_code->setScaledContents(true);
        }
#endif

        ui->widgetPaymentInformation->setEnabled(true);

        ui->address_content->setText(info.address);
        SendCoinsRecipient _info;
        _info.address = info.address;
        QString _uri = GUIUtil::formatBitcoinURI(_info);
        ui->uri_content->setText(_uri);
        ui->uri_content->setToolTip(uri);
    }
    else
    {
        clear();
    }

    QWidget::update();
}

void ReceiveRequestDialog::on_btnCopyURI_clicked()
{
    GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(info));
}

void ReceiveRequestDialog::on_btnCopyAddress_clicked()
{
    GUIUtil::setClipboard(info.address);
}

void ReceiveRequestDialog::on_btnRefreshAddress_clicked()
{
    // Refresh address
    if(refreshAddress())
        update();
}

void ReceiveRequestDialog::on_btnRequestPayment_clicked()
{
    if(requestPaymentDialog->exec() == QDialog::Accepted)
    {
        setInfo(requestPaymentDialog->getInfo());
    }
}

void ReceiveRequestDialog::on_btnClear_clicked()
{
    clear();
}

void ReceiveRequestDialog::clear()
{
    if(getDefaultAddress())
    {
        update();
    }
    else
    {
        setWindowTitle(tr("Request payment to %1").arg(""));
        info = SendCoinsRecipient();
#ifdef USE_QRCODE
        ui->qr_code->clear();
#endif
        ui->uri_content->clear();
        ui->address_content->clear();
        ui->widgetPaymentInformation->setEnabled(false);
    }
}

void ReceiveRequestDialog::reject()
{
    clear();
    QDialog::reject();
}

void ReceiveRequestDialog::accept()
{
    clear();
    QDialog::accept();
}
