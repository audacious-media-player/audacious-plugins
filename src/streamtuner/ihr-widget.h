// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#ifndef STREAMTUNER_IHR_WIDGET_H
#define STREAMTUNER_IHR_WIDGET_H

#include <libaudqt/treeview.h>

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QSplitter>
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "ihr-model.h"

class IHRListingWidget : public audqt::TreeView {
public:
     IHRListingWidget(QWidget * parent = nullptr);

     void activate (const QModelIndex & index);

private:
     IHRTunerModel *m_model;
};

class IHRMarketWidget : public QTreeView {
public:
    IHRMarketWidget(QWidget * parent = nullptr);

private:
    IHRMarketModel *m_model;
};

class IHRTunerWidget : public QWidget {
public:
    IHRTunerWidget(QWidget * parent = nullptr);

private:
    IHRListingWidget *m_tuner;
    IHRMarketWidget *m_markets;
    QSplitter *m_splitter;
    QVBoxLayout *m_layout;
};

#endif
