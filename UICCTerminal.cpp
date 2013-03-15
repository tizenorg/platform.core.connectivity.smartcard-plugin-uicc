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

/* standard library header */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

/* SLP library header */

/* local header */
#include "Debug.h"
#include "TerminalInterface.h"
#include "UICCTerminal.h"

#ifndef EXPORT_API
#define EXPORT_API __attribute__((visibility("default")))
#endif

typedef struct _callback_param_t
{
	void *callback;
	void *param;
}
callback_param_t;

using namespace smartcard_service_api;

static const char *se_name = "SIM1";

/* below functions will be called when dlopen or dlclose is called */
void __attribute__ ((constructor)) lib_init()
{
}

void __attribute__ ((destructor)) lib_fini()
{
}

/* below trhee functions must be implemented */
extern "C" EXPORT_API const char *get_name()
{
	return se_name;
}

extern "C" EXPORT_API void *create_instance()
{
	return (void *)UICCTerminal::getInstance();
}

extern "C" EXPORT_API void destroy_instance(void *instance)
{
	UICCTerminal *inst = (UICCTerminal *)instance;
	if (inst == UICCTerminal::getInstance())
	{
		inst->finalize();
	}
	else
	{
		SCARD_DEBUG_ERR("instance is invalid : getInstance [%p], instance [%p]", UICCTerminal::getInstance(), instance);
	}
}

static void _uiccTransmitCallback(TapiHandle *handle, int result, void *data, void *user_data)
{
	TelSimAccessResult_t access_rt = (TelSimAccessResult_t)result;
	TelSimApduResp_t *apdu = (TelSimApduResp_t *)data;
	callback_param_t *param = (callback_param_t *)user_data;

	SCARD_DEBUG("APDU response");

	if (param != NULL)
	{
		if (param->callback != NULL)
		{
			terminalTransmitCallback callback = (terminalTransmitCallback)param->callback;

			if (apdu != NULL && apdu->apdu_resp_len > 0)
			{
				callback(apdu->apdu_resp, apdu->apdu_resp_len, access_rt, param->param);
			}
			else
			{
				callback(NULL, 0, access_rt, param->param);
			}
		}
		else
		{
			SCARD_DEBUG("there is no callback");
		}

		delete param;
	}
	else
	{
		SCARD_DEBUG_ERR("invalid param");
	}
}

static void _uiccGetATRCallback(TapiHandle *handle, int result, void *data, void *user_data)
{
	TelSimAccessResult_t access_rt = (TelSimAccessResult_t)result;
	TelSimAtrResp_t *atr  = (TelSimAtrResp_t *)data;
	callback_param_t *param = (callback_param_t *)user_data;

	SCARD_DEBUG("APDU response");

	if (param != NULL)
	{
		if (param->callback != NULL)
		{
			terminalGetATRCallback callback = (terminalGetATRCallback)param->callback;

			if (atr != NULL && atr->atr_resp_len > 0)
			{
				callback(atr->atr_resp, atr->atr_resp_len, access_rt, param->param);
			}
			else
			{
				callback(NULL, 0, access_rt, param->param);
			}
		}
		else
		{
			SCARD_DEBUG("there is no callback");
		}

		delete param;
	}
	else
	{
		SCARD_DEBUG_ERR("invalid param");
	}
}

namespace smartcard_service_api
{
	UICCTerminal::UICCTerminal()
	{
		name = (char *)se_name;
		initialize();
	}

	UICCTerminal::~UICCTerminal()
	{
		finalize();
	}

	UICCTerminal *UICCTerminal::getInstance()
	{
		static UICCTerminal instance;

		return &instance;
	}

	bool UICCTerminal::initialize()
	{
		SCARD_BEGIN();

		if (initialized == false)
		{
			char **cpList = NULL;

//			cpList = tel_get_cp_name_list();

			handle = tel_init(NULL);
			if (handle != NULL)
			{
				int error;

				error = tel_register_noti_event(handle, TAPI_NOTI_SIM_STATUS, &UICCTerminal::uiccStatusNotiCallback, this);

				initialized = true;
			}
			else
			{
				SCARD_DEBUG_ERR("tel_init failed");
			}
		}

		SCARD_END();

		return initialized;
	}

	void UICCTerminal::finalize()
	{
		SCARD_BEGIN();

		if (isInitialized())
		{
			tel_deregister_noti_event(handle, TAPI_NOTI_SIM_STATUS);

			tel_deinit(handle);

			initialized = false;
		}

		SCARD_END();
	}

	int UICCTerminal::transmitSync(ByteArray command, ByteArray &response)
	{
		int result = -1;

		SCARD_BEGIN();

		SCOPE_LOCK(mutex)
		{
			if (command.getLength() > 0)
			{
				TelSimApdu_t apdu_data = { 0, };

				apdu_data.apdu = command.getBuffer();
				apdu_data.apdu_len = command.getLength();

				syncLock();

				result = tel_req_sim_apdu(handle, &apdu_data, &UICCTerminal::uiccTransmitAPDUCallback, this);
				if (result == 0)
				{
					SCARD_DEBUG("tel_req_sim_apdu request is success");

					error = 0;
					this->response.releaseBuffer();

					result = waitTimedCondition(3);


					if (result == 0 && error == 0)
					{
						if (this->response.getLength() > 0)
						{
							response = this->response;
						}

						SCARD_DEBUG("tel_req_sim_apdu success, length [%d]", response.getLength());
					}
					else
					{
						SCARD_DEBUG_ERR("tel_req_sim_apdu failed, result [%d], cbResult [%d]", result, error);
					}
				}
				else
				{
					SCARD_DEBUG_ERR("tel_req_sim_apdu failed [%d]", result);
				}

				syncUnlock();
			}
			else
			{
				SCARD_DEBUG_ERR("apdu is empty");
			}
		}

		SCARD_END();

		return result;
	}

	int UICCTerminal::getATRSync(ByteArray &atr)
	{
		int result = 0;

		SCARD_BEGIN();

		SCOPE_LOCK(mutex)
		{
			syncLock();

			result = tel_req_sim_atr(handle, &UICCTerminal::uiccGetAtrCallback, this);
			if (result == 0)
			{
				SCARD_DEBUG("tel_req_sim_atr request is success");

				error = 0;
				this->response.releaseBuffer();

				result = waitTimedCondition(3);

				if (result == 0 && error == 0)
				{
					if (this->response.getLength() > 0)
					{
						atr = this->response;
					}

					SCARD_DEBUG("tel_req_sim_atr success, length [%d]", response.getLength());
				}
				else
				{
					SCARD_DEBUG_ERR("tel_req_sim_atr failed, result [%d], cbResult [%d]", result, error);
				}
			}
			else
			{
				SCARD_DEBUG_ERR("tel_req_sim_atr failed [%d]", result);
			}

			syncUnlock();
		}

		SCARD_END();

		return result;
	}

	int UICCTerminal::transmit(ByteArray command, terminalTransmitCallback callback, void *userParam)
	{
		int result = -1;

		SCARD_BEGIN();

		SCOPE_LOCK(mutex)
		{
			if (command.getLength() > 0)
			{
				TelSimApdu_t apdu_data = { 0, };
				callback_param_t *param = NULL;

				apdu_data.apdu = command.getBuffer();
				apdu_data.apdu_len = command.getLength();

				param = new callback_param_t();
				param->callback = (void *)callback;
				param->param = userParam;

				result = tel_req_sim_apdu(handle, &apdu_data, _uiccTransmitCallback, param);
				if (result == 0)
				{
					SCARD_DEBUG("tel_req_sim_apdu request is success");
				}
				else
				{
					SCARD_DEBUG_ERR("tel_req_sim_apdu failed [%d]", result);
				}
			}
			else
			{
				SCARD_DEBUG_ERR("apdu is empty");
			}
		}

		SCARD_END();

		return result;
	}

	int UICCTerminal::getATR(terminalGetATRCallback callback, void *userParam)
	{
		int result = 0;

		SCARD_BEGIN();

		SCOPE_LOCK(mutex)
		{
			callback_param_t *param = NULL;

			param = new callback_param_t();
			param->callback = (void *)callback;
			param->param = userParam;

			result = tel_req_sim_atr(handle, _uiccGetATRCallback, param);
			if (result == 0)
			{
				SCARD_DEBUG("tel_req_sim_atr request is success");
			}
			else
			{
				SCARD_DEBUG_ERR("tel_req_sim_atr failed [%d]", result);
			}
		}

		SCARD_END();

		return result;
	}

	bool UICCTerminal::isSecureElementPresence()
	{
		bool result = false;
		int error = 0;
		TelSimCardStatus_t state = (TelSimCardStatus_t)0;
		int cardChanged = 0;

		SCARD_BEGIN();

		error = tel_get_sim_init_info(handle, &state, &cardChanged);

		SCARD_DEBUG("current sim init state = [%d], error [%d], cardChanged [%d]", state, error, cardChanged);

		if (error == 0)
		{
			if (state == TAPI_SIM_STATUS_SIM_INIT_COMPLETED || state == TAPI_SIM_STATUS_SIM_INITIALIZING)
			{
				SCARD_DEBUG("sim is initialized");

				result = true;
			}
			else
			{
				SCARD_DEBUG_ERR("sim is not initialized");
			}
		}
		else
		{
			SCARD_DEBUG_ERR("error = [%d]", error);
		}

		SCARD_END();

		return result;
	}

	void UICCTerminal::uiccTransmitAPDUCallback(TapiHandle *handle, int result, void *data, void *user_data)
	{
		UICCTerminal *instance = (UICCTerminal *)user_data;
		TelSimAccessResult_t access_rt = (TelSimAccessResult_t)result;
		TelSimApduResp_t *apdu = (TelSimApduResp_t *)data;

		SCARD_DEBUG("APDU response");

		instance->syncLock();

		instance->error = access_rt;

		if (instance->error == 0)
		{
			if (apdu != NULL && apdu->apdu_resp_len > 0)
			{
				instance->response.setBuffer(apdu->apdu_resp, apdu->apdu_resp_len);
			}

			SCARD_DEBUG("response : %s", instance->response.toString());
		}
		else
		{
			SCARD_DEBUG_ERR("error : event->Status == [%d]", access_rt);
		}

		instance->signalCondition();
		instance->syncUnlock();
	}

	void UICCTerminal::uiccGetAtrCallback(TapiHandle *handle, int result, void *data, void *user_data)
	{
		UICCTerminal *instance = (UICCTerminal *)user_data;
		TelSimAccessResult_t access_rt = (TelSimAccessResult_t)result;
		TelSimAtrResp_t *atr  = (TelSimAtrResp_t *)data;

		SCARD_DEBUG("Get ATR response");

		instance->syncLock();

		instance->error = access_rt;

		if (access_rt == 0)
		{
			if (atr != NULL && atr->atr_resp_len > 0)
			{
				instance->response.setBuffer(atr->atr_resp, atr->atr_resp_len);
			}

			SCARD_DEBUG("response : %s", instance->response.toString());
		}
		else
		{
			SCARD_DEBUG_ERR("error : event->Status == [%d]", access_rt);
		}

		instance->signalCondition();
		instance->syncUnlock();
	}

	void UICCTerminal::uiccStatusNotiCallback(TapiHandle *handle, const char *noti_id, void *data, void *user_data)
	{
		UICCTerminal *instance = (UICCTerminal *)user_data;
		TelSimCardStatus_t *status = (TelSimCardStatus_t *)data;

		SCARD_DEBUG("TAPI_NOTI_SIM_STATUS");

		switch (*status)
		{
		case TAPI_SIM_STATUS_SIM_INIT_COMPLETED :
			SCARD_DEBUG("TAPI_SIM_STATUS_SIM_INIT_COMPLETED");

			if (instance->statusCallback != NULL)
			{
				instance->statusCallback((void *)se_name, NOTIFY_SE_AVAILABLE, 0, NULL);
			}
			break;

		case TAPI_SIM_STATUS_CARD_REMOVED :
			SCARD_DEBUG("TAPI_SIM_STATUS_CARD_REMOVED");

			if (instance->statusCallback != NULL)
			{
				instance->statusCallback((void *)se_name, NOTIFY_SE_NOT_AVAILABLE, 0, NULL);
			}
			break;

		default :
			SCARD_DEBUG("unknown status [%d]", *status);
			break;
		}
	}
} /* namespace smartcard_service_api */
