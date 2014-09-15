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

#include <opendatacon/IUI.h>
class WebUI : public IUI
{
public:
	WebUI(uint16_t port);
	~WebUI();
    
    /* Implement IUI interface */
    void AddJsonResponder(const std::string name, std::weak_ptr<const IJsonResponder> responder);
	int start();
	void stop();

	/* HTTP response handler call back */
    int
		http_ahc(void *cls,
	struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size, void **ptr);
    
private:
    const int port;
    std::string cert_pem;
    std::string key_pem;
    
    /* */
    int CreateNewRequest(void *cls,
                                struct MHD_Connection *connection,
                                const char *url,
                                const char *method,
                                const char *version,
                                const char *upload_data,
                                size_t *upload_data_size,
                                void **con_cls);
    
	/* HTTP file handler */
    int ReturnFile(struct MHD_Connection *connection, const char *url);
    int ReturnJSON(struct MHD_Connection *connection, const std::string& url, const ParamCollection& params);
	struct MHD_Daemon * d;
    
    /* JSON response handlers */
    std::unordered_map<std::string, std::shared_ptr<const IJsonResponder>> LocalResponders;
    std::unordered_map<std::string, std::weak_ptr<const IJsonResponder>> JsonResponders;
};
