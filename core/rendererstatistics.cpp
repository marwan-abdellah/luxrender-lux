/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#include "rendererstatistics.h"
#include "context.h"

#include <algorithm>
#include <limits>

#include <boost/regex.hpp>
#include <boost/format.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
using boost::posix_time::time_duration;

namespace lux
{

RendererStatistics::RendererStatistics()
	: Queryable("renderer_statistics"),
	formattedLong(NULL),
	formattedShort(NULL),
	windowStartTime(0.0)
{
	AddDoubleAttribute(*this, "elapsedTime", "Elapsed rendering time", &RendererStatistics::getElapsedTime);
	AddDoubleAttribute(*this, "remainingTime", "Remaining rendering time", &RendererStatistics::getRemainingTime);
	AddDoubleAttribute(*this, "haltTime", "Halt rendering after time", &RendererStatistics::getHaltTime);
	AddDoubleAttribute(*this, "percentHaltTimeComplete", "Percent of halt time completed", &RendererStatistics::getPercentHaltTimeComplete);
	AddDoubleAttribute(*this, "percentComplete", "Percent of render completed", &RendererStatistics::getPercentComplete);
	AddDoubleAttribute(*this, "efficiency", "Efficiency of renderer", &RendererStatistics::getEfficiency);

	AddIntAttribute(*this, "threadCount", "Number of rendering threads on local node", &RendererStatistics::getThreadCount);
	AddIntAttribute(*this, "slaveNodeCount", "Number of network slave nodes", &RendererStatistics::getSlaveNodeCount);
}

void RendererStatistics::reset() {
	boost::mutex::scoped_lock window_mutex(windowMutex);
	
	resetDerived();

	timer.Reset();
	windowStartTime = 0.0;
}

void RendererStatistics::updateStatisticsWindow() {
	boost::mutex::scoped_lock window_mutex(windowMutex);

	updateStatisticsWindowDerived();

	windowStartTime = getElapsedTime();
}

// Returns halttime if set, otherwise infinity
double RendererStatistics::getHaltTime() {
	int haltTime = 0;

	Queryable* filmRegistry = Context::GetActive()->registry["film"];
	if (filmRegistry)
		haltTime = (*filmRegistry)["haltTime"].IntValue();

	return haltTime > 0 ? haltTime : std::numeric_limits<double>::infinity();
}

// Returns percent of halttime completed, zero if halttime is not set
double RendererStatistics::getPercentHaltTimeComplete() {
	return (getElapsedTime() / getHaltTime()) * 100.0;
}

// Returns time remaining until halttime, infinity if halttime is not set
double RendererStatistics::getRemainingTime() {
	return (std::max)(0.0, getHaltTime() - getElapsedTime());
}

double RendererStatistics::getPercentComplete() {
	return getPercentHaltTimeComplete();
}

u_int RendererStatistics::getSlaveNodeCount() {
	return Context::GetActive()->GetServerCount();
}

RendererStatistics::Formatted::Formatted(RendererStatistics* rs, const std::string& name)
	: Queryable(name),
	rs(rs)
{
	AddStringAttribute(*this, "_recommended_string", "Recommended statistics string", &RendererStatistics::Formatted::getRecommendedString);
	AddStringAttribute(*this, "_recommended_string_template", "Recommended statistics string template", &RendererStatistics::Formatted::getRecommendedStringTemplate);

	AddStringAttribute(*this, "elapsedTime", "Elapsed rendering time", &RendererStatistics::Formatted::getElapsedTime);
	AddStringAttribute(*this, "remainingTime", "Remaining rendering time", &RendererStatistics::Formatted::getRemainingTime);
	AddStringAttribute(*this, "haltTime", "Halt rendering after time", &RendererStatistics::Formatted::getHaltTime);
}

// Helper class for RendererStatistics::Formatted::getStringFromTemplate()
class AttributeFormatter {
public:
	AttributeFormatter(Queryable& q) : obj(q) {}

	std::string operator()(boost::smatch m) {
		// attribute in first capture subgroup
		std::string attr_name = m[1];
		return m[1].str().length() > 0 ? obj[attr_name].StringValue() : "%";
	}

private:
	Queryable& obj;
};

std::string RendererStatistics::Formatted::getStringFromTemplate(const std::string& t)
{
	AttributeFormatter fmt(*this);
	boost::regex attrib_expr("%([^%]*)%");

	return boost::regex_replace(t, attrib_expr, fmt, boost::match_default | boost::format_all);
}

std::string RendererStatistics::Formatted::getRecommendedString() {
	return getStringFromTemplate(getRecommendedStringTemplate());
}

std::string RendererStatistics::Formatted::getElapsedTime() {
	return boost::posix_time::to_simple_string(time_duration(0, 0, static_cast<time_duration::sec_type>(rs->getElapsedTime()), 0));
}

std::string RendererStatistics::Formatted::getRemainingTime() {
	return boost::posix_time::to_simple_string(time_duration(0, 0, static_cast<time_duration::sec_type>(rs->getRemainingTime()), 0));
}

std::string RendererStatistics::Formatted::getHaltTime() {
	return boost::posix_time::to_simple_string(time_duration(0, 0, static_cast<time_duration::sec_type>(rs->getHaltTime()), 0));
}

RendererStatistics::FormattedLong::FormattedLong(RendererStatistics* rs)
	: Formatted(rs, "renderer_statistics_formatted")
{
	AddStringAttribute(*this, "percentHaltTimeComplete", "Percent of halt time completed", &RendererStatistics::FormattedLong::getPercentHaltTimeComplete);
	AddStringAttribute(*this, "percentComplete", "Percent of render completed", &RendererStatistics::FormattedLong::getPercentComplete);

	AddStringAttribute(*this, "efficiency", "Efficiency of renderer", &RendererStatistics::FormattedLong::getEfficiency);

	AddStringAttribute(*this, "threadCount", "Number of rendering threads on local node", &RendererStatistics::FormattedLong::getThreadCount);
	AddStringAttribute(*this, "slaveNodeCount", "Number of network slave nodes", &RendererStatistics::FormattedLong::getSlaveNodeCount);
}

std::string RendererStatistics::FormattedLong::getRecommendedStringTemplate() {
	std::string stringTemplate = "%elapsedTime%";
	if (rs->getHaltTime() != std::numeric_limits<double>::infinity())
		stringTemplate += " [%remainingTime%] (%percentHaltTimeComplete%)";
	stringTemplate += " - %threadCount%";
	if (rs->getSlaveNodeCount() != 0)
		stringTemplate += " %slaveNodeCount%";

	return stringTemplate;
}

std::string RendererStatistics::FormattedLong::getPercentComplete() {
	return boost::str(boost::format("%1$0.0f%% Complete") % rs->getPercentComplete());
}

std::string RendererStatistics::FormattedLong::getPercentHaltTimeComplete() {
	return boost::str(boost::format("%1$0.0f%% Time Complete") % rs->getPercentHaltTimeComplete());
}

std::string RendererStatistics::FormattedLong::getEfficiency() {
	return boost::str(boost::format("%1$0.0f%% Efficiency") % rs->getEfficiency());
}

std::string RendererStatistics::FormattedLong::getThreadCount() {
	u_int tc = rs->getThreadCount();
	return boost::str(boost::format("%1% %2%") % tc % Pluralize("Thread", tc));
}

std::string RendererStatistics::FormattedLong::getSlaveNodeCount() {
	u_int snc = rs->getSlaveNodeCount();
	return boost::str(boost::format("%1% %2%") % snc % Pluralize("Node", snc));
}

RendererStatistics::FormattedShort::FormattedShort(RendererStatistics* rs)
	: Formatted(rs, "renderer_statistics_formatted_short")
{
	AddStringAttribute(*this, "percentHaltTimeComplete", "Percent of halt time completed", &RendererStatistics::FormattedShort::getPercentHaltTimeComplete);
	AddStringAttribute(*this, "percentComplete", "Percent of render completed", &RendererStatistics::FormattedShort::getPercentComplete);

	AddStringAttribute(*this, "efficiency", "Efficiency of renderer", &RendererStatistics::FormattedShort::getEfficiency);

	AddStringAttribute(*this, "threadCount", "Number of rendering threads on local node", &RendererStatistics::FormattedShort::getThreadCount);
	AddStringAttribute(*this, "slaveNodeCount", "Number of network slave nodes", &RendererStatistics::FormattedShort::getSlaveNodeCount);
}

std::string RendererStatistics::FormattedShort::getRecommendedStringTemplate() {
	std::string stringTemplate = "%elapsedTime%";
	if (rs->getHaltTime() != std::numeric_limits<double>::infinity())
		stringTemplate += " [%remainingTime%] (%percentHaltTimeComplete%)";
	stringTemplate += " - %threadCount%";
	if (rs->getSlaveNodeCount() != 0)
		stringTemplate += " %slaveNodeCount%";

	return stringTemplate;
}

std::string RendererStatistics::FormattedShort::getPercentComplete() {
	return boost::str(boost::format("%1$0.0f%% Cmplt") % rs->getPercentComplete());
}

std::string RendererStatistics::FormattedShort::getPercentHaltTimeComplete() {
	return boost::str(boost::format("%1$0.0f%% T Cmplt") % rs->getPercentHaltTimeComplete());
}

std::string RendererStatistics::FormattedShort::getEfficiency() {
	return boost::str(boost::format("%1$0.0f%% Eff") % rs->getEfficiency());
}

std::string RendererStatistics::FormattedShort::getThreadCount() {
	return boost::str(boost::format("%1% T") % rs->getThreadCount());
}

std::string RendererStatistics::FormattedShort::getSlaveNodeCount() {
	return boost::str(boost::format("%1% N") % rs->getSlaveNodeCount());
}

// Generic functions
std::string Pluralize(const std::string& l, u_int v) {
	return (v == 1) ? l : (l.compare(l.size() - 1, 1, "s")) ? l + "s" : l + "es";
}

double MagnitudeReduce(double number) {
	if (isnan(number) || isinf(number))
		return number;

	if (number < 2e3)
		return number;

	if (number < 2e6)
		return number / 1e3;

	if (number < 2e9)
		return number / 1e6;

	if (number < 2e12)
		return number / 1e9;

	return number / 1e12;
}

const char* MagnitudePrefix(double number) {
	if (isnan(number) || isinf(number))
		return "";

	if (number < 2e3)
		return "";

	if (number < 2e6)
		return "k";

	if (number < 2e9)
		return "M";

	if (number < 2e12)
		return "G";

	return "T";
}

} // namespace lux

