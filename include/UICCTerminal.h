/*
 * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UICCTERMINAL_H_
#define UICCTERMINAL_H_

/* standard library header */
#include <vector>
#include <map>

/* SLP library header */
#include "tapi_common.h"
#include "ITapiSim.h"

/* local header */
#include "Lock.h"
#include "Terminal.h"

using namespace std;

namespace smartcard_service_api
{
	class UICCTerminal: public Terminal
	{
	private:
		PMutex mutex;
		TapiHandle *handle;

		/* temporary data for sync function */
		ByteArray response;
		int error;
		bool complete;

		UICCTerminal();
		~UICCTerminal();

		static void uiccTransmitAPDUCallback(TapiHandle *handle, int result, void *data, void *user_data);
		static void uiccGetAtrCallback(TapiHandle *handle, int result, void *data, void *user_data);
		static void uiccStatusNotiCallback(TapiHandle *handle, const char *noti_id, void *data, void *user_data);

	public:
		static UICCTerminal *getInstance();

		bool initialize();
		void finalize();

		bool isSecureElementPresence() const;

		int transmitSync(const ByteArray &command, ByteArray &response);
		int getATRSync(ByteArray &atr);

		int transmit(const ByteArray &command, terminalTransmitCallback callback, void *userParam);
		int getATR(terminalGetATRCallback callback, void *userParam);

		friend void uiccTransmitAPDUCallback(TapiHandle *handle, int result, void *data, void *user_data);
		friend void uiccGetAtrCallback(TapiHandle *handle, int result, void *data, void *user_data);
		friend void uiccStatusNotiCallback(TapiHandle *handle, const char *noti_id, void *data, void *user_data);
	};

} /* namespace smartcard_service_api */
#endif /* UICCTERMINAL_H_ */
