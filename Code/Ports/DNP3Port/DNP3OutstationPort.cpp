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
 * DNP3ServerPort.cpp
 *
 *  Created on: 15/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "ChannelStateSubscriber.h"
#include "DNP3OutstationPort.h"
#include "DNP3PortConf.h"
#include "OpenDNP3Helpers.h"
#include "TypeConversion.h"
#include <opendnp3/outstation/UpdateBuilder.h>
#include <opendnp3/outstation/Updates.h>
#include <opendnp3/master/IUTCTimeSource.h>
#include <chrono>
#include <iostream>
#include <opendatacon/util.h>
#include <opendnp3/outstation/IOutstationApplication.h>
#include <opendnp3/logging/LogLevels.h>
#include <regex>

DNP3OutstationPort::DNP3OutstationPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DNP3Port(aName, aConfFilename, aConfOverrides),
	pOutstation(nullptr)
{}

DNP3OutstationPort::~DNP3OutstationPort()
{
	if(pOutstation)
	{
		pOutstation->Shutdown();
		pOutstation.reset();
	}
	ChannelStateSubscriber::Unsubscribe(pChanH.get());
	pChanH.reset();
}

void DNP3OutstationPort::Enable()
{
	if(enabled)
		return;
	if(nullptr == pOutstation)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: DNP3 stack not configured.", Name);

		return;
	}
	pOutstation->Enable();
	enabled = true;

	PublishEvent(ConnectState::PORT_UP);
}
void DNP3OutstationPort::Disable()
{
	if(!enabled)
		return;
	enabled = false;

	pOutstation->Disable();
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: DNP3 stack disabled", Name);
}

// Called by OpenDNP3 Thread Pool
// Called when a the reset/unreset status of the link layer changes (and on link up / channel down)
void DNP3OutstationPort::OnStateChange(opendnp3::LinkStatus status)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: LinkStatus {}.", Name, opendnp3::LinkStatusSpec::to_human_string(status));
	pChanH->SetLinkStatus(status);
	//TODO: track a new statistic - reset count
}
// Called by OpenDNP3 Thread Pool
// Called when a keep alive message (request link status) receives no response
void DNP3OutstationPort::OnKeepAliveFailure()
{
	last_link_down_time = msSinceEpoch();
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveFailure() called.", Name);
	pChanH->LinkDown();
}
//There's no callback if a link recovers after it goes down,
//	we keep track of the last time a keepalive failed
//	if there hasn't been a fail in longer than the configured keepalive period, we're back up.
void DNP3OutstationPort::LinkUpCheck()
{
	if(!enabled)
		return;
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: LinkUpCheck() called.", Name);
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->pPointConf->LinkKeepAlivems != 0)
	{
		auto ms_since_down = msSinceEpoch() - last_link_down_time;
		auto ms_required = pConf->pPointConf->LinkKeepAlivems + pConf->pPointConf->LinkTimeoutms;
		if(ms_since_down >= ms_required)
		{
			if(auto log = odc::spdlog_get("DNP3Port"))
				log->debug("{}: LinkUpCheck() success - link is back up.", Name);
			pChanH->LinkUp();
			return;
		}

		auto ms_left = ms_required - ms_since_down;
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: LinkUpCheck() failure - link is still down. {}ms until next check.", Name, ms_left);
		pLinkUpCheckTimer->expires_from_now(std::chrono::milliseconds(ms_left));
		pLinkUpCheckTimer->async_wait([this](asio::error_code err)
			{
				//if the channel went down in the mean time, timer would be cancelled
				//double check LinkDeadness in case handler was already Q'd
				if(!err && pChanH->GetLinkDeadness() == LinkDeadness::LinkDownChannelUp)
					LinkUpCheck();
			});
	}
}
void DNP3OutstationPort::LinkDeadnessChange(LinkDeadness from, LinkDeadness to)
{
	if(to == LinkDeadness::LinkUpChannelUp) //must be on link up
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: Link up.", Name);
		PublishEvent(ConnectState::CONNECTED);
	}

	if(from == LinkDeadness::LinkUpChannelUp) //must be on link down
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->debug("{}: Link down.", Name);
		PublishEvent(ConnectState::DISCONNECTED);

		if(to == LinkDeadness::LinkDownChannelUp) //means keepalive failed
			LinkUpCheck();                      //kick off checking for coming back up
	}

	//if we get here, it's not link up or down, it's a channel up or down
	if(to == LinkDeadness::LinkDownChannelDown)
		pLinkUpCheckTimer->cancel();
}
void DNP3OutstationPort::ChannelWatchdogTrigger(bool on)
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: ChannelWatchdogTrigger({}) called.", Name, on);
	if(enabled)
	{
		if(on)
			pOutstation->Disable();
		else
			pOutstation->Enable();
	}
}

// Called by OpenDNP3 Thread Pool
// Called when a keep alive message receives a valid response
void DNP3OutstationPort::OnKeepAliveSuccess()
{
	if(auto log = odc::spdlog_get("DNP3Port"))
		log->debug("{}: KeepAliveSuccess() called.", Name);
	pChanH->LinkUp();
}

TCPClientServer DNP3OutstationPort::ClientOrServer()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(pConf->mAddrConf.ClientServer == TCPClientServer::DEFAULT)
		return TCPClientServer::SERVER;
	return pConf->mAddrConf.ClientServer;
}

void DNP3OutstationPort::Build()
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if (!pChanH->SetChannel())
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Channel not found for outstation.", Name);
		return;
	}

	opendnp3::OutstationStackConfig StackConfig;
	for (uint16_t index : pConf->pPointConf->AnalogIndicies)
	{
		StackConfig.database.analog_input[index].clazz = pConf->pPointConf->AnalogClasses[index];
		StackConfig.database.analog_input[index].evariation = pConf->pPointConf->EventAnalogResponses[index];
		StackConfig.database.analog_input[index].svariation = pConf->pPointConf->StaticAnalogResponses[index];
		StackConfig.database.analog_input[index].deadband = pConf->pPointConf->AnalogDeadbands[index];
	}
	for (uint16_t index : pConf->pPointConf->BinaryIndicies)
	{
		StackConfig.database.binary_input[index].clazz = pConf->pPointConf->BinaryClasses[index];
		StackConfig.database.binary_input[index].evariation = pConf->pPointConf->EventBinaryResponses[index];
		StackConfig.database.binary_input[index].svariation = pConf->pPointConf->StaticBinaryResponses[index];
	}

	// Link layer configuration
	opendnp3::LinkConfig link(false,pConf->pPointConf->LinkUseConfirms);
	link.LocalAddr = pConf->mAddrConf.OutstationAddr;
	link.RemoteAddr = pConf->mAddrConf.MasterAddr;
	link.Timeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->LinkTimeoutms);
	if(pConf->pPointConf->LinkKeepAlivems == 0)
		link.KeepAliveTimeout = opendnp3::TimeDuration::Max();
	else
		link.KeepAliveTimeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->LinkKeepAlivems);
	StackConfig.link = link;

	// Outstation parameters
	StackConfig.outstation.params.allowUnsolicited = pConf->pPointConf->EnableUnsol;
	StackConfig.outstation.params.unsolClassMask = pConf->pPointConf->GetUnsolClassMask();
	StackConfig.outstation.params.typesAllowedInClass0 = opendnp3::StaticTypeBitField::AllTypes();                                      /// TODO: Create parameter
	StackConfig.outstation.params.maxControlsPerRequest = pConf->pPointConf->MaxControlsPerRequest;                                     /// The maximum number of controls the outstation will attempt to process from a single APDU
	StackConfig.outstation.params.maxTxFragSize = pConf->pPointConf->MaxTxFragSize;                                                     /// The maximum fragment size the outstation will use for fragments it sends
	StackConfig.outstation.params.maxRxFragSize = pConf->pPointConf->MaxTxFragSize;                                                     /// The maximum fragment size the outstation will use for fragments it sends
	StackConfig.outstation.params.selectTimeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->SelectTimeoutms);             /// How long the outstation will allow an operate to proceed after a prior select
	StackConfig.outstation.params.solConfirmTimeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->SolConfirmTimeoutms);     /// Timeout for solicited confirms
	StackConfig.outstation.params.unsolConfirmTimeout = opendnp3::TimeDuration::Milliseconds(pConf->pPointConf->UnsolConfirmTimeoutms); /// Timeout for unsolicited confirms

	// TODO: Expose event limits for any new event types to be supported by opendatacon
	StackConfig.outstation.eventBufferConfig.maxBinaryEvents = pConf->pPointConf->MaxBinaryEvents;   /// The number of binary events the outstation will buffer before overflowing
	StackConfig.outstation.eventBufferConfig.maxAnalogEvents = pConf->pPointConf->MaxAnalogEvents;   /// The number of analog events the outstation will buffer before overflowing
	StackConfig.outstation.eventBufferConfig.maxCounterEvents = pConf->pPointConf->MaxCounterEvents; /// The number of counter events the outstation will buffer before overflowing

	//FIXME?: hack to create a toothless shared_ptr
	//	this is needed because the main exe manages our memory
	auto wont_free = std::shared_ptr<DNP3OutstationPort>(this,[](void*){});
	auto pCommandHandle = std::dynamic_pointer_cast<opendnp3::ICommandHandler>(wont_free);
	auto pApplication = std::dynamic_pointer_cast<opendnp3::IOutstationApplication>(wont_free);

	pOutstation = pChanH->GetChannel()->AddOutstation(Name, pCommandHandle, pApplication, StackConfig);

	if (pOutstation == nullptr)
	{
		if(auto log = odc::spdlog_get("DNP3Port"))
			log->error("{}: Error creating outstation.", Name);
		return;
	}

	pStateSync = pIOS->make_strand();
}

//DataPort function for UI
const Json::Value DNP3OutstationPort::GetCurrentState() const
{
	std::atomic_bool stateExecuted(false);
	Json::Value temp_value;
	pStateSync->post([this, &stateExecuted, &temp_value]()
		{
			temp_value = state;
			stateExecuted = true;
		});

	while (!stateExecuted)
	{
		pIOS->poll_one();
	}

	return temp_value;
}

//DataPort function for UI
const Json::Value DNP3OutstationPort::GetStatistics() const
{
	Json::Value event;
	if (auto pChan = pChanH->GetChannel())
	{
		auto ChanStats = pChan->GetStatistics();
		event["parser"]["numHeaderCrcError"] = Json::UInt(ChanStats.parser.numHeaderCrcError);
		event["parser"]["numBodyCrcError"] = Json::UInt(ChanStats.parser.numBodyCrcError);
		event["parser"]["numLinkFrameRx"] = Json::UInt(ChanStats.parser.numLinkFrameRx);
		event["parser"]["numBadLength"] = Json::UInt(ChanStats.parser.numBadLength);
		event["parser"]["numBadFunctionCode"] = Json::UInt(ChanStats.parser.numBadFunctionCode);
		event["parser"]["numBadFCV"] = Json::UInt(ChanStats.parser.numBadFCV);
		event["parser"]["numBadFCB"] = Json::UInt(ChanStats.parser.numBadFCB);
		event["channel"]["numOpen"] = Json::UInt(ChanStats.channel.numOpen);
		event["channel"]["numOpenFail"] = Json::UInt(ChanStats.channel.numOpenFail);
		event["channel"]["numClose"] = Json::UInt(ChanStats.channel.numClose);
		event["channel"]["numBytesRx"] = Json::UInt(ChanStats.channel.numBytesRx);
		event["channel"]["numBytesTx"] = Json::UInt(ChanStats.channel.numBytesTx);
		event["channel"]["numLinkFrameTx"] = Json::UInt(ChanStats.channel.numLinkFrameTx);
	}
	if (pOutstation != nullptr)
	{
		auto StackStats = this->pOutstation->GetStackStatistics();
		event["link"]["numBadMasterBit"] = Json::UInt(StackStats.link.numBadMasterBit);
		event["link"]["numUnexpectedFrame"] = Json::UInt(StackStats.link.numUnexpectedFrame);
		event["link"]["numUnknownDestination"] = Json::UInt(StackStats.link.numUnknownDestination);
		event["link"]["numUnknownSource"] = Json::UInt(StackStats.link.numUnknownSource);
		event["transport"]["numTransportBufferOverflow"] = Json::UInt(StackStats.transport.rx.numTransportBufferOverflow);
		event["transport"]["numTransportDiscard"] = Json::UInt(StackStats.transport.rx.numTransportDiscard);
		event["transport"]["numTransportErrorRx"] = Json::UInt(StackStats.transport.rx.numTransportErrorRx);
		event["transport"]["numTransportIgnore"] = Json::UInt(StackStats.transport.rx.numTransportIgnore);
		event["transport"]["numTransportRx"] = Json::UInt(StackStats.transport.rx.numTransportRx);
		event["transport"]["numTransportTx"] = Json::UInt(StackStats.transport.tx.numTransportTx);
	}

	return event;
}

template<typename T>
inline opendnp3::CommandStatus DNP3OutstationPort::SupportsT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if(std::is_same<T,opendnp3::ControlRelayOutputBlock>::value) //TODO: add support for other types of controls (probably un-templatise when we support more)
	{
		for(auto index : pConf->pPointConf->ControlIndicies)
			if(index == aIndex)
				return opendnp3::CommandStatus::SUCCESS;
	}
	return opendnp3::CommandStatus::NOT_SUPPORTED;
}

// Called by OpenDNP3 Thread Pool
template<typename T>
inline opendnp3::CommandStatus DNP3OutstationPort::PerformT(T& arCommand, uint16_t aIndex)
{
	if(!enabled)
		return opendnp3::CommandStatus::UNDEFINED;

	auto event = ToODC(arCommand, aIndex, Name);

	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());
	if (!pConf->pPointConf->WaitForCommandResponses)
	{
		PublishEvent(event);
		return opendnp3::CommandStatus::SUCCESS;
	}

	//TODO: enquire about the possibility of the opendnp3 API having a callback for the result
	// Or if the outstation supported Group13Var1/2 (BinaryCommandEvent), we could use that (maybe the latest opendnp3 already does...?)
	// either one would avoid the below polling loop
	std::atomic_bool cb_executed(false);
	CommandStatus cb_status;
	auto StatusCallback = std::make_shared<std::function<void (CommandStatus status)>>([&cb_status,&cb_executed](CommandStatus status)
		{
			cb_status = status;
			cb_executed = true;
		});
	PublishEvent(event, StatusCallback);
	while(!cb_executed)
	{
		//This loop pegs a core and blocks the outstation strand,
		//	but there's no other way to wait for the result.
		//	We can maybe do some work while we wait.
		pIOS->poll_one();
	}
	return FromODC(cb_status);
}

void DNP3OutstationPort::Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if (!enabled)
	{
		(*pStatusCallback)(CommandStatus::UNDEFINED);
		return;
	}

	switch(event->GetEventType())
	{
		case EventType::Binary:
			SetState("BinaryCurrent", std::to_string(event->GetIndex()), event->GetPayloadString());
			SetState("BinaryQuality", std::to_string(event->GetIndex()), ToString(event->GetQuality()));
			EventT(FromODC<opendnp3::Binary>(event), event->GetIndex());
			break;
		case EventType::Analog:
			SetState("AnalogCurrent", std::to_string(event->GetIndex()), event->GetPayloadString());
			SetState("AnalogQuality", std::to_string(event->GetIndex()), ToString(event->GetQuality()));
			EventT(FromODC<opendnp3::Analog>(event), event->GetIndex());
			break;
		case EventType::BinaryQuality:
			SetState("BinaryQuality", std::to_string(event->GetIndex()), event->GetPayloadString());
			EventQ<opendnp3::Binary>(FromODC<opendnp3::BinaryQuality>(event), event->GetIndex(), opendnp3::FlagsType::BinaryInput);
			break;
		case EventType::AnalogQuality:
			SetState("AnalogQuality", std::to_string(event->GetIndex()), event->GetPayloadString());
			EventQ<opendnp3::Analog>(FromODC<opendnp3::AnalogQuality>(event), event->GetIndex(), opendnp3::FlagsType::AnalogInput);
			break;
		case EventType::ConnectState:
			break;
		default:
			(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
			return;
	}
	(*pStatusCallback)(CommandStatus::SUCCESS);
}

template<typename T, typename Q>
inline void DNP3OutstationPort::EventQ(Q qual, uint16_t index, opendnp3::FlagsType FT)
{
	opendnp3::UpdateBuilder builder;
	builder.Modify(FT, index, index, static_cast<uint8_t>(qual));
	pOutstation->Apply(builder.Build());
}

template<typename T>
inline void DNP3OutstationPort::EventT(T meas, uint16_t index)
{
	auto pConf = static_cast<DNP3PortConf*>(this->pConf.get());

	if (
		(pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ALWAYS) ||
		((pConf->pPointConf->TimestampOverride == DNP3PointConf::TimestampOverride_t::ZERO) && (meas.time.value == 0))
		)
	{
		meas.time = opendnp3::DNPTime(msSinceEpoch());
	}

	opendnp3::UpdateBuilder builder;
	builder.Update(meas, index);
	pOutstation->Apply(builder.Build());
}

inline void DNP3OutstationPort::SetState(const std::string& type, const std::string& index, const std::string& payload)
{
	pStateSync->post([=]()
		{
			state[type][index] = payload;
		});
}
