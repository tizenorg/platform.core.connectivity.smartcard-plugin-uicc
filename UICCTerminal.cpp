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
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>

/* SLP library header */

/* local header */
#include "smartcard-types.h"
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

typedef struct _context_s
{
	GMainLoop *loop;
	void *resp;
}
context_s;

void __attribute__ ((constructor)) lib_init()
{
}

void __attribute__ ((destructor)) lib_fini()
{
}

/* below three functions must be implemented */
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
		_ERR("instance is invalid : getInstance [%p], instance [%p]",
			UICCTerminal::getInstance(), instance);
	}
}

static void _uiccTransmitCallback(TapiHandle *handle, int result,
	void *data, void *user_data)
{
	TelSimAccessResult_t access_rt = (TelSimAccessResult_t)result;
	TelSimApduResp_t *apdu = (TelSimApduResp_t *)data;
	callback_param_t *param = (callback_param_t *)user_data;

	if (param != NULL)
	{
		if (param->callback != NULL)
		{
			terminalTransmitCallback callback =
				(terminalTransmitCallback)param->callback;

			if (apdu != NULL && apdu->apdu_resp_len > 0)
			{
				callback(apdu->apdu_resp, apdu->apdu_resp_len,
					access_rt, param->param);
			}
			else
			{
				callback(NULL, 0, access_rt, param->param);
			}
		}

		delete param;
	}
	else
	{
		_ERR("invalid param");
	}
}

static void _uiccGetATRCallback(TapiHandle *handle, int result,
	void *data, void *user_data)
{
	TelSimAccessResult_t access_rt = (TelSimAccessResult_t)result;
	TelSimAtrResp_t *atr  = (TelSimAtrResp_t *)data;
	callback_param_t *param = (callback_param_t *)user_data;

	if (param != NULL)
	{
		if (param->callback != NULL)
		{
			terminalGetATRCallback callback =
				(terminalGetATRCallback)param->callback;

			if (atr != NULL && atr->atr_resp_len > 0)
			{
				callback(atr->atr_resp, atr->atr_resp_len,
					access_rt, param->param);
			}
			else
			{
				callback(NULL, 0, access_rt, param->param);
			}
		}

		delete param;
	}
	else
	{
		_ERR("invalid param");
	}
}

static void _uiccCallback_sync(TapiHandle *handle, int result,
	void *data, void *user_data)
{
	context_s* context = (context_s*)user_data;

	if(result == TAPI_SIM_ACCESS_SUCCESS)
		context->resp = data;

	g_main_loop_quit(context->loop);
}

namespace smartcard_service_api
{
	UICCTerminal::UICCTerminal() : Terminal()
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
		_BEGIN();

		if (initialized == false)
		{
//			char **cpList = NULL;
//
//			cpList = tel_get_cp_name_list();

			handle = tel_init(NULL);
			if (handle != NULL)
			{
				int error;

				error = tel_register_noti_event(handle,
					TAPI_NOTI_SIM_STATUS,
					&UICCTerminal::uiccStatusNotiCallback,
					this);
				if (error < 0) {
					_ERR("tel_register_noti_event failed, [%d]", error);
				}

				initialized = true;
			}
			else
			{
				_ERR("tel_init failed");
			}
		}

		_END();

		return initialized;
	}

	void UICCTerminal::finalize()
	{
		_BEGIN();

		if (isInitialized())
		{
			if (tel_deregister_noti_event(handle,
				TAPI_NOTI_SIM_STATUS) < 0) {
				_ERR("tel_deregister_noti_event failed");
			}

			tel_deinit(handle);

			initialized = false;
		}

		_END();
	}

	bool UICCTerminal::open()
	{
		/* no need to open */
		return true;
	}

	void UICCTerminal::close()
	{
		/* no need to close */
	}

	int UICCTerminal::transmitSync(const ByteArray &command,
		ByteArray &response)
	{
		int result;

		_BEGIN();

		SCOPE_LOCK(mutex)
		{
			if (command.size() > 0)
			{
				TelSimApduResp_t *resp = NULL;
				TelSimApdu_t apdu_data = { 0, };
				context_s context = { NULL, resp };

				context.loop = g_main_loop_new(NULL, false);

				apdu_data.apdu = (uint8_t *)command.getBuffer();
				apdu_data.apdu_len = command.size();

				result = tel_req_sim_apdu(handle, &apdu_data,
					_uiccCallback_sync, &context);
				if (result != 0)
				{
					_ERR("tel_req_sim_apdu failed [%d]", result);

					result = SCARD_ERROR_IO_FAILED;
				}
				else
				{
					g_main_loop_run(context.loop);
					response.assign(((TelSimApduResp_t *)context.resp)->apdu_resp,
						((TelSimApduResp_t *)context.resp)->apdu_resp_len);
				}
			}
			else
			{
				_ERR("apdu is empty");

				result = SCARD_ERROR_ILLEGAL_PARAM;
			}
		}

		_END();

		return result;
	}

	int UICCTerminal::getATRSync(ByteArray &atr)
	{
		int result;

		_BEGIN();

		SCOPE_LOCK(mutex)
		{
			TelSimAtrResp_t *resp = NULL;
			context_s context = { NULL, resp };

			context.loop = g_main_loop_new(NULL, false);

			result = tel_req_sim_atr(handle,
				_uiccCallback_sync, &context);
			if(result != 0)
			{
				_ERR("tel_req_sim_atr failed [%d]", result);

				result = SCARD_ERROR_IO_FAILED;
			}
			else
			{
				g_main_loop_run(context.loop);

				if((TelSimAtrResp_t *)context.resp != NULL &&
						((TelSimAtrResp_t *)context.resp)->atr_resp != NULL )
				{
					atr.assign(((TelSimAtrResp_t *)context.resp)->atr_resp,
						((TelSimAtrResp_t *)context.resp)->atr_resp_len);
				}
			}
		}

		_END();

		return result;
	}

	int UICCTerminal::transmit(const ByteArray &command,
		terminalTransmitCallback callback, void *userParam)
	{
		int result;

		_BEGIN();

		SCOPE_LOCK(mutex)
		{
			if (command.size() > 0)
			{
				TelSimApdu_t apdu_data = { 0, };
				callback_param_t *param = NULL;

				apdu_data.apdu = (uint8_t *)command.getBuffer();
				apdu_data.apdu_len = command.size();

				param = new callback_param_t();
				param->callback = (void *)callback;
				param->param = userParam;

				result = tel_req_sim_apdu(handle, &apdu_data,
					_uiccTransmitCallback, param);
				if (result == 0)
				{
					_DBG("tel_req_sim_apdu request is success");
				}
				else
				{
					_ERR("tel_req_sim_apdu failed [%d]", result);
					result = SCARD_ERROR_IO_FAILED;
				}
			}
			else
			{
				_ERR("apdu is empty");
				result = SCARD_ERROR_ILLEGAL_PARAM;
			}
		}

		_END();

		return result;
	}

	int UICCTerminal::getATR(terminalGetATRCallback callback,
		void *userParam)
	{
		int result;

		_BEGIN();

		SCOPE_LOCK(mutex)
		{
			callback_param_t *param = NULL;

			param = new callback_param_t();
			param->callback = (void *)callback;
			param->param = userParam;

			result = tel_req_sim_atr(handle,
				_uiccGetATRCallback, param);
			if (result == 0)
			{
				_DBG("tel_req_sim_atr request is success");
			}
			else
			{
				_ERR("tel_req_sim_atr failed [%d]", result);
				result = SCARD_ERROR_IO_FAILED;
			}
		}

		_END();

		return result;
	}

	bool UICCTerminal::isSecureElementPresence() const
	{
		bool result = false;
		int error = 0;
		TelSimCardStatus_t state = (TelSimCardStatus_t)0;
		int cardChanged = 0;

		_BEGIN();

		error = tel_get_sim_init_info(handle, &state, &cardChanged);
		if (error == 0)
		{
			if (state == TAPI_SIM_STATUS_SIM_INIT_COMPLETED)
			{
				result = true;
			}
			else
			{
				_ERR("sim is not initialized, state [%d]", state);
			}
		}
		else
		{
			_ERR("tel_get_sim_init_info failed, [%d]", error);
		}

		_END();

		return result;
	}

	void UICCTerminal::uiccStatusNotiCallback(TapiHandle *handle,
		const char *noti_id, void *data, void *user_data)
	{
		UICCTerminal *instance = (UICCTerminal *)user_data;
		TelSimCardStatus_t *status = (TelSimCardStatus_t *)data;

		switch (*status)
		{
		case TAPI_SIM_STATUS_SIM_INIT_COMPLETED :
			_DBG("TAPI_SIM_STATUS_SIM_INIT_COMPLETED");

			if (instance->statusCallback != NULL)
			{
				instance->statusCallback((void *)se_name,
					NOTIFY_SE_AVAILABLE, 0, NULL);
			}
			break;

		case TAPI_SIM_STATUS_CARD_REMOVED :
			_DBG("TAPI_SIM_STATUS_CARD_REMOVED");

			if (instance->statusCallback != NULL)
			{
				instance->statusCallback((void *)se_name,
					NOTIFY_SE_NOT_AVAILABLE, 0, NULL);
			}
			break;

		default :
			/* ignore notification */
			break;
		}
	}
} /* namespace smartcard_service_api */
