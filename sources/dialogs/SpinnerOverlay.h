#pragma once

#include <QLabel>
#include <QMovie>

// A simple RAII helper for showing an animated-GIF overlay
class SpinnerOverlay {
public:
    /// parent: the widget to overlay
    /// gifPath: resource path or filesystem path to your .gif
    /// size: desired width/height of the spinner in px
    SpinnerOverlay(QWidget* parent,
        const QString& gifPath = ":/media/gray_circles_rotate.gif",
        int size = 240)
        : _parent(parent),
        _label(new QLabel(parent)),
        _movie(new QMovie(gifPath, QByteArray(), parent))
    {
        // Prepare label
        _label->setAttribute(Qt::WA_TransparentForMouseEvents);
        _label->setAttribute(Qt::WA_NoSystemBackground);
        _label->setAttribute(Qt::WA_TranslucentBackground);
        _label->setAlignment(Qt::AlignCenter);

        // Prepare movie
        _movie->setCacheMode(QMovie::CacheAll);
        _movie->setScaledSize(QSize(size, size));
        _label->setMovie(_movie);

        // Position and show
        centerOnParent(size);
        _label->show();
        _movie->start();
    }

    ~SpinnerOverlay() {
        _movie->stop();
        _label->hide();
    }

private:
    void centerOnParent(int size) {
        if (!_parent) return;
        const int x = (_parent->width() - size) / 2;
        const int y = (_parent->height() - size) / 2;
        _label->setGeometry(x, y, size, size);
    }

    QWidget* _parent;
    QLabel* _label;
    QMovie* _movie;
};
