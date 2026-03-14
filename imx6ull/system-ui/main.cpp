#include "mainwindow.h"

#include <QApplication>
#include <QFile>

/*
 * 模块说明：Qt 程序入口。
 * 运行流程：创建应用对象，加载全局 QSS，显示主窗口并进入事件循环。
 */

/*
 * 程序入口：初始化 QApplication、装载全局样式并启动主窗口。
 * 说明：这里不承载业务逻辑，只负责把 UI 运行环境准备好。
 */
int main(int argc, char *argv[])
{
    /* Qt 应用对象 */
    QApplication a(argc, argv);
    /* 指定文件 */
    QFile file(":/style.qss");

    /* 判断文件是否存在 */
    if (file.exists() ) {
        /* 以只读的方式打开 */
        file.open(QFile::ReadOnly);
        /* 以字符串的方式保存读出的结果 */
        QString styleSheet = QLatin1String(file.readAll());
        /* 设置全局样式 */
        qApp->setStyleSheet(styleSheet);
        /* 关闭文件 */
        file.close();
    }

    MainWindow w;
    /* 显示主窗口 */
    w.show();
    return a.exec();
}
