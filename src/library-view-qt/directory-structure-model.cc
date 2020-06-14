#include "directory-structure-model.h"

DirectoryStructureModel::DirectoryStructureModel(QObject *parent)
	: QFileSystemModel(parent) {}

QModelIndex DirectoryStructureModel::set_root_path(QString path) {
    if(rootPath() == path)return index(rootPath());
    beginResetModel();
    auto index = setRootPath(path);
    endResetModel();
    return index;
}

int DirectoryStructureModel::columnCount(const QModelIndex &index) const {
	return 1;
}
