// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "qr/qr_generate.h"

#include "base/assertion.h"

#include "QrCode.hpp"
#include <QtGui/QPainter>
#include <string>

namespace Qr {
namespace {

using namespace qrcodegen;

[[nodiscard]] int ReplaceElements(const Data &data) {
	const auto elements = (data.size / 4);
	const auto shift = (data.size - elements) % 2;
	return (elements - shift);
}

} // namespace

Data Encode(const QString &text) {
	Expects(!text.isEmpty());

	auto result = Data();
	const auto utf8 = text.toStdString();
	const auto qr = QrCode::encodeText(utf8.c_str(), QrCode::Ecc::MEDIUM);
	result.size = qr.getSize();
	Assert(result.size > 0);

	result.values.reserve(result.size * result.size);
	for (auto row = 0; row != result.size; ++row) {
		for (auto column = 0; column != result.size; ++column) {
			result.values.push_back(qr.getModule(row, column));
		}
	}
	return result;
}

void PrepareForRound(QPainter &p) {
	p.setRenderHints(QPainter::Antialiasing
		| QPainter::SmoothPixmapTransform
		| QPainter::TextAntialiasing
		| QPainter::HighQualityAntialiasing);
	p.setPen(Qt::NoPen);
}

QImage GenerateSingle(int size, Qt::GlobalColor bg, Qt::GlobalColor color) {
	auto result = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
	result.fill(bg);
	{
		auto p = QPainter(&result);
		PrepareForRound(p);
		p.setBrush(color);
		p.drawRoundedRect(
			QRect{ 0, 0, size, size },
			size / 2.,
			size / 2.);
	}
	return result;
}

int ReplaceSize(const Data &data, int pixel) {
	return ReplaceElements(data) * pixel;
}

QImage Generate(const Data &data, int pixel) {
	Expects(data.size > 0);
	Expects(data.values.size() == data.size * data.size);

	const auto replaceElements = ReplaceElements(data);
	const auto replaceFrom = (data.size - replaceElements) / 2;
	const auto replaceTill = (data.size - replaceFrom);
	const auto black = GenerateSingle(pixel, Qt::white, Qt::black);
	const auto white = GenerateSingle(pixel, Qt::black, Qt::white);
	const auto value = [&](int row, int column) {
		return (row >= 0)
			&& (row < data.size)
			&& (column >= 0)
			&& (column < data.size)
			&& (row < replaceFrom
				|| row >= replaceTill
				|| column < replaceFrom
				|| column >= replaceTill)
			&& data.values[row * data.size + column];
	};
	const auto blackFull = [&](int row, int column) {
		return (value(row - 1, column) && value(row + 1, column))
			|| (value(row, column - 1) && value(row, column + 1));
	};
	const auto whiteCorner = [&](int row, int column, int dx, int dy) {
		return !value(row + dy, column)
			|| !value(row, column + dx)
			|| !value(row + dy, column + dx);
	};
	const auto whiteFull = [&](int row, int column) {
		return whiteCorner(row, column, -1, -1)
			&& whiteCorner(row, column, 1, -1)
			&& whiteCorner(row, column, 1, 1)
			&& whiteCorner(row, column, -1, 1);
	};
	auto result = QImage(
		data.size * pixel,
		data.size * pixel,
		QImage::Format_ARGB32_Premultiplied);
	{
		auto p = QPainter(&result);
		const auto skip = pixel - pixel / 2;
		const auto brect = [&](int x, int y, int width, int height) {
			p.fillRect(x, y, width, height, Qt::black);
		};
		const auto wrect = [&](int x, int y, int width, int height) {
			p.fillRect(x, y, width, height, Qt::white);
		};
		const auto large = [&](int x, int y) {
			p.setBrush(Qt::black);
			p.drawRoundedRect(
				QRect{ x, y, pixel * 7, pixel * 7 },
				pixel * 2.,
				pixel * 2.);
			p.setBrush(Qt::white);
			p.drawRoundedRect(
				QRect{ x + pixel, y + pixel, pixel * 5, pixel * 5 },
				pixel * 1.5,
				pixel * 1.5);
			p.setBrush(Qt::black);
			p.drawRoundedRect(
				QRect{ x + pixel * 2, y + pixel * 2, pixel * 3, pixel * 3 },
				pixel,
				pixel);

		};
		for (auto row = 0; row != data.size; ++row) {
			for (auto column = 0; column != data.size; ++column) {
				if ((row < 7 && (column < 7 || column >= data.size - 7))
					|| (column < 7 && (row < 7 || row >= data.size - 7))) {
					continue;
				}
				const auto x = column * pixel;
				const auto y = row * pixel;
				const auto index = row * data.size + column;
				const auto fill = data.values[index] ? Qt::black : Qt::white;
				if (value(row, column)) {
					if (blackFull(row, column)) {
						brect(x, y, pixel, pixel);
					} else {
						p.drawImage(x, y, black);
						if (value(row - 1, column)) {
							brect(x, y, pixel, pixel / 2);
						} else if (value(row + 1, column)) {
							brect(x, y + skip, pixel, pixel / 2);
						}
						if (value(row, column - 1)) {
							brect(x, y, pixel / 2, pixel);
						} else if (value(row, column + 1)) {
							brect(x + skip, y, pixel / 2, pixel);
						}
					}
				} else if (whiteFull(row, column)) {
					wrect(x, y, pixel, pixel);
				} else {
					p.drawImage(x, y, white);
					if (whiteCorner(row, column, -1, -1)
						&& whiteCorner(row, column, 1, -1)) {
						wrect(x, y, pixel, pixel / 2);
					} else if (whiteCorner(row, column, -1, 1)
						&& whiteCorner(row, column, 1, 1)) {
						wrect(x, y + skip, pixel, pixel / 2);
					}
					if (whiteCorner(row, column, -1, -1)
						&& whiteCorner(row, column, -1, 1)) {
						wrect(x, y, pixel / 2, pixel);
					} else if (whiteCorner(row, column, 1, -1)
						&& whiteCorner(row, column, 1, 1)) {
						wrect(x + skip, y, pixel / 2, pixel);
					}
					if (whiteCorner(row, column, -1, -1)) {
						wrect(x, y, pixel / 2, pixel / 2);
					}
					if (whiteCorner(row, column, 1, -1)) {
						wrect(x + skip, y, pixel / 2, pixel / 2);
					}
					if (whiteCorner(row, column, 1, 1)) {
						wrect(x + skip, y + skip, pixel / 2, pixel / 2);
					}
					if (whiteCorner(row, column, -1, 1)) {
						wrect(x, y + skip, pixel / 2, pixel / 2);
					}
				}
			}
		}

		PrepareForRound(p);
		large(0, 0);
		large((data.size - 7) * pixel, 0);
		large(0, (data.size - 7) * pixel);
	}
	return result;
}

QImage ReplaceCenter(QImage qr, const QImage &center) {
	{
		auto p = QPainter(&qr);
		const auto x = (qr.width() - center.width()) / 2;
		const auto y = (qr.height() - center.height()) / 2;
		p.drawImage(x, y, center);
	}
	return qr;
}

} // namespace Qr