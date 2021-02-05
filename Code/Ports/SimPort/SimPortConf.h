/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
/*
 * SimPortConf.h
 *
 *  Created on: 2015-12-16
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef SIMPORTCONF_H
#define SIMPORTCONF_H

#include "SimPortData.h"
#include "sqlite3/sqlite3.h"
#include <opendatacon/DataPortConf.h>
#include <opendatacon/IOTypes.h>
#include <json/json.h>
#include <memory>

using namespace odc;

using DB_STATEMENT = std::shared_ptr<sqlite3_stmt>;

enum class TimestampMode : uint8_t
{
	FIRST       = 1,
	ABSOLUTE_T  = 1<<1,
	FASTFORWARD = 1<<2,
	TOD         = 1<<3
};

namespace odc
{
ENABLE_BITWISE(TimestampMode)
}

// Hide some of the code to make Logging cleaner
#define LOGTRACE(...) \
	if (auto log = odc::spdlog_get("SimPort")) \
	log->trace(__VA_ARGS__)
#define LOGDEBUG(...) \
	if (auto log = odc::spdlog_get("SimPort")) \
	log->debug(__VA_ARGS__)
#define LOGERROR(...) \
	if (auto log = odc::spdlog_get("SimPort")) \
	log->error(__VA_ARGS__)
#define LOGWARN(...) \
	if (auto log = odc::spdlog_get("SimPort"))  \
	log->warn(__VA_ARGS__)
#define LOGINFO(...) \
	if (auto log = odc::spdlog_get("SimPort")) \
	log->info(__VA_ARGS__)

class SimPortConf: public DataPortConf
{
public:
	SimPortConf();

	void ProcessElements(const Json::Value& json_root);
	std::unordered_map<std::string, DB_STATEMENT> GetDBStats() const;
	TimestampMode TimestampHandling() const;
	void Name(const std::string& name);
	double DefaultStdDev() const;

	std::string HttpAddress() const;
	std::string HttpPort() const;
	std::string Version() const;

	double StdDev(std::size_t index) const;

	void Event(std::shared_ptr<odc::EventInfo> event);
	std::shared_ptr<odc::EventInfo> Event(odc::EventType type, std::size_t index) const;
	void SetCurrentBinaryControl(std::shared_ptr<odc::EventInfo> event, std::size_t index);
	void Payload(odc::EventType type, std::size_t index, double payload);
	double Payload(odc::EventType type, std::size_t index) const;
	double StartValue(odc::EventType type, std::size_t index) const;
	void ForcedState(odc::EventType type, std::size_t index, bool value);
	bool ForcedState(odc::EventType type, std::size_t index) const;
	void UpdateInterval(odc::EventType type, std::size_t index, std::size_t value);
	std::size_t UpdateInterval(odc::EventType type, std::size_t) const;
	Json::Value CurrentState() const;
	std::string CurrentState(odc::EventType type, std::vector<std::size_t>& indexes) const;
	void Timer(const std::string& name, ptimer_t ptr);
	ptimer_t Timer(const std::string& name) const;
	void CancelTimers();
	bool IsIndex(odc::EventType type, std::size_t index) const;
	std::vector<std::size_t> Indexes(odc::EventType type) const;

	std::vector<std::shared_ptr<BinaryFeedback>> BinaryFeedbacks(std::size_t index) const;
	std::shared_ptr<BinaryPosition> GetBinaryPosition(std::size_t index) const;

private:
	std::string m_name;
	TimestampMode m_timestamp_handling;
	std::unordered_map<std::string, DB_STATEMENT> m_db_stats;
	std::shared_ptr<SimPortData> m_pport_data;

	bool m_ParseIndexes(const Json::Value& data, std::size_t& start, std::size_t& stop) const;

	void m_ProcessAnalogs(const Json::Value& analogs);
	void m_ProcessBinaries(const Json::Value& binaires);
	void m_ProcessBinaryControls(const Json::Value& binary_controls);
	void m_ProcessSQLite3(const Json::Value& sqlite, std::size_t index);
	void m_ProcessFeedbackBinaries(const Json::Value& feedback_binaries, std::size_t index,
		std::size_t update_interval);
	void m_ProcessFeedbackPosition(const Json::Value& feedback_position, std::size_t index);
};

#endif // SIMPORTCONF_H
