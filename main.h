/* Create man pages from doxygen XML output.
 *
 * Georg Sauthoff <mail@georg.so>, 2012
 *
 * License: GPLv3+
 *
 */

#ifndef MAIN_H
#define MAIN_H

#include <QObject>

class Main : public QObject {
  Q_OBJECT
  private:
  public:
  public slots:
    void run();
};


#endif
