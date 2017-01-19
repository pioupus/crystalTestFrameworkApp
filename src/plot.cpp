#include "plot.h"

#include <QFileDialog>
#include <QSplitter>
#include <fstream>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>

Plot::Plot(QSplitter *parent)
	: plot(new QwtPlot)
	, curve(new QwtPlotCurve) {
	parent->addWidget(plot.get());
	curve->attach(plot.get());
	plot->setContextMenuPolicy(Qt::ContextMenuPolicy::ActionsContextMenu);

	save_as_csv_action.setText(QObject::tr("save_as_csv"));
	QObject::connect(&save_as_csv_action, &QAction::triggered, [this] {
		auto file = QFileDialog::getSaveFileName(plot.get(), QObject::tr("Select File to save data in"), "date.csv", "*.csv");
		if (file.isEmpty() == false) {
			std::ofstream f{file.toStdString()};
			for (std::size_t i = 0; i < xvalues.size(); i++) {
				f << xvalues[i] << ',' << yvalues[i] << '\n';
			}
		}
	});
	plot->addAction(&save_as_csv_action);
}

void Plot::add(double x, double y) {
	xvalues.push_back(x);
	yvalues.push_back(y);
	update();
}

void Plot::add(const std::vector<int> &data) {
	resize(data.size());
	std::transform(std::begin(data), std::end(data), std::begin(yvalues), std::begin(yvalues), std::plus<>());
}

void Plot::clear() {
	xvalues.clear();
	yvalues.clear();
	update();
}

void Plot::update() {
	curve->setRawSamples(xvalues.data(), yvalues.data(), xvalues.size());
	plot->replot();
}

void Plot::resize(std::size_t size) {
	if (xvalues.size() > size) {
		xvalues.resize(size);
		yvalues.resize(size);
	} else if (xvalues.size() < size) {
		auto old_size = xvalues.size();
		xvalues.resize(size);
		for (auto i = old_size; i < size; i++) {
			xvalues[i] = i;
		}
		yvalues.resize(size, 0.);
	}
}
