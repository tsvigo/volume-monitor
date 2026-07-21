// Volume Monitor — resizable rainbow dial, saves size via PipeWire + Qt5
#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QProcess>
#include <QFont>
#include <QDateTime>
#include <QSettings>
#include <QWheelEvent>
#include <cmath>

class VolumeMonitor : public QWidget {
    double m_vol = 0;
    bool m_muted = false;
    double m_peak = 0;

public:
    VolumeMonitor() {
        setWindowTitle("Volume Control");
        setMinimumSize(200, 200);

        QSettings s("VolumeMonitor", "VolumeMonitor");
        int w = s.value("width", 420).toInt();
        int h = s.value("height", 480).toInt();
        int x = s.value("x", -1).toInt();
        int y = s.value("y", -1).toInt();
        resize(w, h);
        if (x >= 0 && y >= 0) move(x, y);

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &VolumeMonitor::tick);
        timer->start(50);
        tick();
    }

    ~VolumeMonitor() {
        QSettings s("VolumeMonitor", "VolumeMonitor");
        s.setValue("width", width());
        s.setValue("height", height());
        s.setValue("x", x());
        s.setValue("y", y());
    }

private:
    void tick() {
        QProcess p;
        p.start("/usr/bin/wpctl", {"get-volume", "@DEFAULT_AUDIO_SINK@"});
        p.waitForFinished(1000);
        QByteArray out = p.readAllStandardOutput();
        QString line = QString::fromUtf8(out).trimmed();
        m_muted = line.contains("MUTED");
        m_vol = 0;
        if (line.startsWith("Volume:")) {
            QString num = line.mid(8).split(' ').first();
            bool ok = false;
            double v = num.toDouble(&ok);
            if (ok) m_vol = v;
        }
        if (m_vol > m_peak) m_peak = m_vol;
        else m_peak += (m_vol - m_peak) * 0.12;
        if (m_peak < 0.002) m_peak = 0;
        update();
    }

    void setVolume(double v) {
        v = qBound(0.0, v, 1.0);
        v = qRound(v * 100) / 100.0;
        QProcess::startDetached("/usr/bin/wpctl",
            {"set-volume", "@DEFAULT_AUDIO_SINK@", QString::number(v, 'f', 2)});
        m_vol = v;
    }

    void toggleMute() {
        QProcess::startDetached("/usr/bin/wpctl",
            {"set-mute", "@DEFAULT_AUDIO_SINK@", "toggle"});
        m_muted = !m_muted;
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton)
            toggleMute();
    }

    void wheelEvent(QWheelEvent *e) override {
        int delta = e->angleDelta().y();
        double step = 0.01;
        if (delta < 0)
            setVolume(m_vol + step);
        else if (delta > 0)
            setVolume(m_vol - step);
    }

    static QColor rainbow(double pos) {
        double h = (1.0 - pos) * 300.0;
        return QColor::fromHsvF(h / 360.0, 0.92, 1.0);
    }

    static void drawPieSeg(QPainter &p, int cx, int cy, int r, double a1Deg, double a2Deg, const QColor &color) {
        int qa1 = (int)(-a1Deg * 16);
        int span = (int)(-(a2Deg - a1Deg) * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPie(QRect(cx - r, cy - r, r * 2, r * 2), qa1, span);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int W = width(), H = height();
        int S = qMin(W, H);

        // Dark background
        QLinearGradient bgGrad(0, 0, 0, H);
        bgGrad.setColorAt(0, QColor(30, 30, 42));
        bgGrad.setColorAt(1, QColor(16, 16, 24));
        p.fillRect(rect(), bgGrad);

        // Title
        QFont tf("Sans", qMax(10, S / 24), QFont::Bold);
        p.setFont(tf);
        p.setPen(QColor(200, 200, 215));
        p.drawText(QRect(0, S / 20, W, S / 10), Qt::AlignCenter, "VOLUME CONTROL");

        // Dial — centered vertically between title and percentage
        int cx = W / 2;
        int textAreaH = S / 7;
        int titleAreaH = S / 7;
        int cy = (titleAreaH + H - textAreaH) / 2;
        int outerR = qMin((H - titleAreaH - textAreaH) / 2 - 15, (W - 40) / 2);
        if (outerR < 25) outerR = 25;

        double startAngle = 220.0;
        double sweepAngle = 260.0;
        int totalSegs = 24;
        int activeSegs = (int)(m_peak * totalSegs);
        double segSweep = sweepAngle / totalSegs;
        double gap = 3.0;

        // Background segments
        for (int i = 0; i < totalSegs; i++) {
            double a1 = startAngle + i * segSweep + gap / 2;
            double a2 = startAngle + (i + 1) * segSweep - gap / 2;
            drawPieSeg(p, cx, cy, outerR, a1, a2, QColor(32, 32, 48));
        }

        // Active rainbow segments
        for (int i = 0; i < activeSegs; i++) {
            double a1 = startAngle + i * segSweep + gap / 2;
            double a2 = startAngle + (i + 1) * segSweep - gap / 2;
            double frac = (double)i / totalSegs;
            QColor c = rainbow(frac);

            int glowR = outerR + qMax(3, S / 70);
            QRadialGradient glow(cx, cy, glowR);
            glow.setColorAt(0.65, QColor(c.red(), c.green(), c.blue(), 45));
            glow.setColorAt(1.0, QColor(c.red(), c.green(), c.blue(), 0));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(QPoint(cx, cy), glowR, glowR);

            drawPieSeg(p, cx, cy, outerR, a1, a2, c);
        }

        // Inner cutout
        int innerCutR = outerR - qMax(7, S / 20);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(18, 18, 26));
        p.drawEllipse(QPoint(cx, cy), innerCutR, innerCutR);

        // Inner ring
        int ringR = innerCutR - qMax(2, S / 70);
        p.setPen(QPen(QColor(45, 45, 60), qMax(1.0, S / 350.0)));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), ringR, ringR);

        // Metallic knob
        int knobR = qMax(10, (int)(innerCutR * 0.42));
        QRadialGradient knobGrad(cx - knobR * 0.1, cy - knobR * 0.1, knobR * 1.1);
        knobGrad.setColorAt(0, QColor(85, 85, 100));
        knobGrad.setColorAt(0.25, QColor(55, 55, 70));
        knobGrad.setColorAt(0.6, QColor(38, 38, 50));
        knobGrad.setColorAt(1, QColor(22, 22, 32));
        p.setPen(QPen(QColor(60, 60, 75), qMax(1.0, S / 280.0)));
        p.setBrush(knobGrad);
        p.drawEllipse(QPoint(cx, cy), knobR, knobR);

        // Concentric rings
        for (int r = knobR - qMax(2, knobR / 10); r > knobR * 0.2; r -= qMax(2, knobR / 10)) {
            p.setPen(QPen(QColor(52, 52, 65, 60), 0.7));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPoint(cx, cy), r, r);
        }

        // Center highlight
        QRadialGradient chGrad(cx - 3, cy - 3, knobR * 0.35);
        chGrad.setColorAt(0, QColor(110, 110, 128, 80));
        chGrad.setColorAt(1, QColor(40, 40, 52, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(chGrad);
        p.drawEllipse(QPoint(cx, cy), (int)(knobR * 0.35), (int)(knobR * 0.35));

        // Knob reflection
        p.setPen(QPen(QColor(130, 130, 148, 45), qMax(1.0, S / 280.0)));
        int refLen = knobR * 0.5;
        p.drawLine(cx - refLen, cy - knobR * 0.6, cx + refLen * 0.7, cy - knobR * 0.6);

        // Indicator notch
        double indAngle = startAngle + m_peak * sweepAngle;
        double indRad = indAngle * M_PI / 180.0;
        int ix1 = cx + (int)((knobR - 2) * cos(indRad));
        int iy1 = cy - (int)((knobR - 2) * sin(indRad));
        int ix2 = cx + (int)((knobR + qMax(3, S / 28)) * cos(indRad));
        int iy2 = cy - (int)((knobR + qMax(3, S / 28)) * sin(indRad));
        p.setPen(QPen(QColor(255, 255, 255, 200), qMax(1.5, S / 180.0)));
        p.drawLine(ix1, iy1, ix2, iy2);

        // --- Percentage text at bottom ---
        QFont pf("Sans", qMax(14, S / 8), QFont::Bold);
        p.setFont(pf);
        QColor pc = m_muted ? QColor(120, 120, 135) : QColor(230, 230, 240);
        p.setPen(pc);
        p.drawText(QRect(0, H - textAreaH, W, textAreaH), Qt::AlignCenter,
            QString("%1%").arg((int)(m_vol * 100)));
    }

    void resizeEvent(QResizeEvent *) override {
        QSettings s("VolumeMonitor", "VolumeMonitor");
        s.setValue("width", width());
        s.setValue("height", height());
        s.setValue("x", x());
        s.setValue("y", y());
    }

    void moveEvent(QMoveEvent *) override {
        QSettings s("VolumeMonitor", "VolumeMonitor");
        s.setValue("x", x());
        s.setValue("y", y());
    }

    void closeEvent(QCloseEvent *e) override {
        QSettings s("VolumeMonitor", "VolumeMonitor");
        s.setValue("width", width());
        s.setValue("height", height());
        QWidget::closeEvent(e);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("VolumeMonitor");
    QCoreApplication::setApplicationName("VolumeMonitor");
    VolumeMonitor w;
    w.show();
    return app.exec();
}
