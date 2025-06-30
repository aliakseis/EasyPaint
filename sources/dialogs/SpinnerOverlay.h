#pragma once

#include <QLabel>
#include <QTimer>

// A simple RAII helper for showing a spinning overlay
class SpinnerOverlay {
public:
  SpinnerOverlay(QWidget* parent, int size = 64, int interval = 50)
    : _parent(parent),
      _label(new QLabel(parent)),
      _timer(new QTimer(parent)),
      _pixmap(":/media/logo/easypaint_64.png"),
      _angle(0)
  {
    // prepare label
    _label->setPixmap(_pixmap);
    _label->setAlignment(Qt::AlignCenter);
    _label->setAttribute(Qt::WA_TransparentForMouseEvents);
    //_label->setFixedSize(size, size);
    centerOnParent();

    // prepare timer
    _timer->setInterval(interval);
    QObject::connect(_timer, &QTimer::timeout, [this]() {
      _angle = (_angle + 10) % 360;
      _label->setPixmap(_pixmap.transformed(QTransform().rotate(_angle)));
    });

    // show
    _label->show();
    _timer->start();
  }

  ~SpinnerOverlay() {
    _timer->stop();
    _label->hide();
  }

private:
  void centerOnParent() {
    if (!_parent) return;
    //const auto pw = _parent->width();
    //const auto ph = _parent->height();
    //const auto cw = _label->width();
    //const auto ch = _label->height();
    //_label->move((pw - cw)/2, (ph - ch)/2);
    _label->setGeometry(_parent->width() / 2 - 64, _parent->height() / 2 - 64, 128, 128);
  }

  QWidget*      _parent;
  QLabel*       _label;
  QTimer*       _timer;
  QPixmap       _pixmap;
  int           _angle;
};
