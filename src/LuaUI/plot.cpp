#include "plot.h"
#include "config.h"
#include "qt_util.h"

#include <QAction>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSettings>
#include <QSplitter>
#include <QtGlobal>
#include <fstream>
#include <qwt_picker_machine.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_picker.h>

///\cond HIDDEN_SYMBOLS
Curve::Curve(QSplitter *, Plot *plot)
    : plot(plot)
    , curve(new QwtPlotCurve)
    , event_filter(new Event_filter(plot->plot->canvas())) {
    curve->attach(plot->plot);
    curve->setTitle("curve" + QString::number(plot->curve_id_counter++));
    plot->curves.push_back(this);
    plot->plot->canvas()->installEventFilter(event_filter);
}

Curve::~Curve() {
    if (plot) {
        auto &curves = plot->curves;
        curves.erase(std::find(std::begin(curves), std::end(curves), this));
        detach();
    }
}
void Curve::add(const std::vector<double> &data) {
    resize(data.size());
    std::transform(std::begin(data), std::end(data), std::begin(yvalues_orig), std::begin(yvalues_orig), std::plus<>());
    update();
}
///\endcond
void Curve::append_point(double x, double y) {
    xvalues.push_back(x);
    yvalues_orig.push_back(y);
    update();
}

void Curve::add_spectrum_at(const unsigned int spectrum_start_channel, const std::vector<double> &data) {
    size_t s = std::max(xvalues.size(), data.size() + spectrum_start_channel);
    resize(s);
    std::transform(std::begin(data), std::end(data), std::begin(yvalues_orig) + spectrum_start_channel, std::begin(yvalues_orig) + spectrum_start_channel,
                   std::plus<>());
    update();
}

void Curve::clear() {
    xvalues.clear();
    yvalues_orig.clear();
    update();
}

void Curve::set_x_axis_offset(double offset) {
    this->offset = offset;
    xvalues.clear();
    resize(yvalues_orig.size());
}

void Curve::set_x_axis_gain(double gain) {
    this->gain = gain;
    xvalues.clear();
    resize(yvalues_orig.size());
    resize(yvalues_plot.size());
}

double Curve::integrate_ci(double integral_start_ci, double integral_end_ci) {
    double result = 0;
    if (integral_start_ci < 0) {
        integral_start_ci = 0;
    }

    if (integral_end_ci < 0) {
        integral_end_ci = 0;
    }
    unsigned int s = round(integral_start_ci);
    unsigned int e = round(integral_end_ci);

    if (s >= yvalues_plot.size()) {
        s = yvalues_plot.size() - 1;
    }

    if (e >= yvalues_plot.size()) {
        e = yvalues_plot.size() - 1;
    }

    for (unsigned int i = s; i < e; i++) {
        result += yvalues_plot[i];
    }
    return result;
}

void Curve::set_median_enable(bool enable) {
    median_enable = enable;
}

void Curve::set_median_kernel_size(unsigned int kernel_size) {
    if (kernel_size & 1) {
        median_kernel_size = kernel_size;
    } else {
        //TODO: Tell the user that the call had no effect?
    }
}

void Curve::set_color(const Color &color) {
    curve->setPen(QColor(color.rgb));
}
///\cond HIDDEN_SYMBOLS
void Curve::set_onetime_click_callback(std::function<void(double, double)> click_callback) {
    event_filter->add_callback([ callback = std::move(click_callback), this ](QEvent * event) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto mouse_event = static_cast<QMouseEvent *>(event);
            const auto &pixel_pos = mouse_event->pos();
            auto x_pos = plot->plot->invTransform(QwtPlot::xBottom, pixel_pos.x());
            auto y_pos = plot->plot->invTransform(QwtPlot::yLeft, pixel_pos.y());
            callback(x_pos, y_pos);
            event_filter->clear();
            return true;
        }
        return false;
    });
}

void Curve::resize(std::size_t size) {
    if (xvalues.size() > size) {
        xvalues.resize(size);
        yvalues_orig.resize(size);
        yvalues_plot.resize(size);
    } else if (xvalues.size() < size) {
        auto old_size = xvalues.size();
        xvalues.resize(size);
        for (auto i = old_size; i < size; i++) {
            xvalues[i] = offset + gain * i;
        }
        yvalues_orig.resize(size, 0.);
        yvalues_plot.resize(size, 0.);
    }
}

void Curve::update() {
    if (median_enable && (median_kernel_size < yvalues_orig.size())) {
        std::vector<double> kernel(median_kernel_size);
        const unsigned int HALF_KERNEL_SIZE = median_kernel_size / 2;

        for (unsigned int i = 0; i < HALF_KERNEL_SIZE; i++) {
            yvalues_plot[i] = yvalues_orig[i];
        }

        for (unsigned int i = yvalues_orig.size() - HALF_KERNEL_SIZE; i < yvalues_orig.size(); i++) {
            yvalues_plot[i] = yvalues_orig[i];
        }

        for (unsigned int i = HALF_KERNEL_SIZE; i < yvalues_orig.size() - HALF_KERNEL_SIZE; i++) {
            for (unsigned int j = 0; j < median_kernel_size; j++) {
                kernel[j] = yvalues_orig[i + j - HALF_KERNEL_SIZE];
            }
            std::sort(kernel.begin(), kernel.end());
            yvalues_plot[i] = kernel[HALF_KERNEL_SIZE];
        }

    } else {
        yvalues_plot = yvalues_orig;
    }
    curve->setRawSamples(xvalues.data(), yvalues_plot.data(), xvalues.size());
    plot->update();
}

void Curve::detach() {
    curve->setSamples(xvalues.data(), yvalues_plot.data(), xvalues.size());
    event_filter->clear();
}

Plot::Plot(QSplitter *parent)
    : plot(new QwtPlot)
    , picker(new QwtPlotPicker(plot->canvas()))
    , track_picker(new QwtPlotPicker(plot->canvas()))
    , clicker(new QwtPickerClickPointMachine)
    , tracker(new QwtPickerTrackerMachine) {
    clicker->setState(clicker->PointSelection);
    parent->addWidget(plot);
    plot->setContextMenuPolicy(Qt::ContextMenuPolicy::ActionsContextMenu);
    set_rightclick_action();
    picker->setStateMachine(clicker);
    picker->setTrackerMode(QwtPicker::ActiveOnly);
    track_picker->setStateMachine(tracker);
    track_picker->setTrackerMode(QwtPicker::AlwaysOn);
}

Plot::~Plot() {
    for (auto &curve : curves) {
        curve->detach();
        curve->plot = nullptr;
    }
    //the plot was using xvalues and yvalues directly, but now they are gone
    //this is to make the plot own the data
}
///\endcond
void Plot::clear() {
    curves.clear();
}

void Plot::set_x_marker(const std::string &title, double xpos, const Color &color) {
    const int Y_AXIS_STEP = 10;
    auto marker = new QwtPlotMarker{title.c_str()};
    if (title.empty() == false) {
        marker->setLabel(QString::fromStdString(title));
    }
    marker->setXValue(xpos);
    marker->setLinePen(QColor(color.rgb));
    marker->setLineStyle(QwtPlotMarker::LineStyle::VLine);
    marker->setLabelOrientation(Qt::Orientation::Vertical);
    marker->attach(plot);
#if 0
    int i = 0;
    double plot_y_min = plot->axisScaleDiv(QwtPlot::Axis::yLeft).lowerBound();
    double plot_y_max = plot->axisScaleDiv(QwtPlot::Axis::yLeft).upperBound();
    double plot_y_range = plot_y_max - plot_y_min;
    //plot_y_range += plot_y_range/Y_AXIS_STEP;
    const QwtPlotItemList &itmList = plot->itemList();
    //qDebug() << "ymarker calc:";
    for (QwtPlotItemIterator it = itmList.begin(); it != itmList.end(); ++it) {
        QwtPlotItem *item = *it;
        if (item->isVisible() && item->rtti() == QwtPlotItem::Rtti_PlotMarker) {
            QwtPlotMarker *m = dynamic_cast<QwtPlotMarker *>(item);
            if (m) {
                i++;
                double y_value = plot_y_min + (i % Y_AXIS_STEP) * plot_y_range / Y_AXIS_STEP;
                //qDebug() << "ymarker: " << y_value;
                m->setYValue(y_value);
            }
        }
    }
#endif
    update();
}

void Plot::update() {
    plot->replot();
}

void Plot::set_rightclick_action() {
    delete save_as_csv_action;
    save_as_csv_action = new QAction(plot);
    save_as_csv_action->setText(QObject::tr("save_as_csv"));
    if (curves.size()) {
        std::vector<QwtPlotCurve *> raw_curves;
        raw_curves.resize(curves.size());
        std::transform(std::begin(curves), std::end(curves), std::begin(raw_curves), [](const Curve *curve) { return curve->curve; });
        QObject::connect(save_as_csv_action, &QAction::triggered, [ plot = this->plot, curves = std::move(raw_curves) ] {
            QString last_dir = QSettings{}.value(Globals::last_csv_saved_directory, QDir::currentPath()).toString();
            auto dir = QFileDialog::getExistingDirectory(plot, QObject::tr("Select folder to save data in"), last_dir);
            if (dir.isEmpty() == false) {
                QSettings{}.setValue(Globals::last_csv_saved_directory, dir);
                for (auto &curve : curves) {
                    auto filename = QDir(dir).filePath(curve->title().text() + ".csv");
                    std::ofstream f{filename.toStdString()};
                    auto data = curve->data();
                    auto size = data->size();
                    for (std::size_t i = 0; i < size; i++) {
                        const auto &point = data->sample(i);
                        f << point.x() << ';' << point.y() << '\n';
                    }
                    f.flush();
                    if (!f) {
                        QMessageBox::critical(plot, QObject::tr("Error"), QObject::tr("Failed writing to file %1").arg(filename));
                    }
                }
            }

        });
    } else {
        save_as_csv_action->setEnabled(false);
    }
    plot->addAction(save_as_csv_action);
}
