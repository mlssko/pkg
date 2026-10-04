#pragma once
#include <QString>

// Информация об одном файле изображения
struct ImageInfo {
    QString path;
    QString name;
    QString format;       // определяется по сигнатуре, а не по расширению
    QString compression;
    QString extra;        // дополнительные сведения
    QString status;       // "OK", "Файл поврежден", "Неизвестный формат"
    int width = 0;
    int height = 0;
    double dpiX = 0;
    double dpiY = 0;
    int depth = 0;        // бит на пиксель
};

// Читает только заголовки файла (без загрузки пикселей) вручную по спецификациям
ImageInfo readImageInfo(const QString &path);
