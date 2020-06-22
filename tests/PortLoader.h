/*	opendatacon
*
*	Copyright (c) 2015:
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
/**
*/

#ifndef PORTLOADER_H
#define PORTLOADER_H

#include <opendatacon/DataPort.h>
#include <opendatacon/Platform.h>

using namespace odc;

typedef DataPort* (*newptr)(const std::string& Name, const std::string& File, const Json::Value& Overrides);
typedef void (*delptr)(DataPort*);

symbol_ptr GetPortFunc(module_ptr pluginlib, const std::string& objname, bool destroy = false);
newptr GetPortCreator(module_ptr pluginlib, const std::string& objname);
delptr GetPortDestroyer(module_ptr pluginlib, const std::string& objname);

#endif
