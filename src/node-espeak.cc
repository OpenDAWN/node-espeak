#include <v8.h>
#include <nan.h>
#include <stdlib.h>
#include <iostream>

#define gender_none 0
#define gender_male 1
#define gender_female 2

#include "../deps/espeak/src/speak_lib.h"

using namespace node;
using namespace v8;

static int samplerate;
static NanCallback *callback;
static bool hasInit = false;
static bool hasCallback;
static unsigned int flags;

static char variant = 0;
static char *language = "en\0";
static char gender = gender_none;
static char age = 30;

static int SynthCallback(short *wav, int numSamples, espeak_EVENT *events) {
	while(events->type != 0) {
		if(events->type == espeakEVENT_SAMPLERATE) {
			samplerate = events->id.number;
		}
		events++;
	}
	if(numSamples > 0) {
		if(hasCallback) {
			const unsigned argc = 3;
			Local<Object> buffer = NanNewBufferHandle(reinterpret_cast<char*>(wav), numSamples*2);
			Local<Value> argv[argc] = { buffer, NanNew<Number>(numSamples), NanNew<Number>(samplerate) };
			callback->Call(argc, argv);
		}
	}
	return 0;
}

static void reloadVoice() {
	espeak_VOICE *voice = new espeak_VOICE();
	voice->gender = gender;
	voice->age = age;
	voice->variant = variant;
	voice->languages = language;
	espeak_SetVoiceByProperties(voice);
}

static NAN_METHOD(GetVoice) {
	NanScope();
	Local<Object> obj = NanNew<Object>();
	if(gender == gender_none) {
		obj->Set(NanNew("gender"), NanNew("none"));
	}
	else if(gender == gender_male) {
		obj->Set(NanNew("gender"), NanNew("male"));
	}
	else if(gender == gender_female) {
		obj->Set(NanNew("gender"), NanNew("female"));
	}
	obj->Set(NanNew("variant"), NanNew(variant));
	obj->Set(NanNew("age"), NanNew(age));
	obj->Set(NanNew("language"), NanNew(std::string(language)));
	NanReturnValue(obj);
}

static NAN_METHOD(GetProperties) {
	NanScope();
	Local<Object> obj = NanNew<Object>();
	obj->Set(NanNew("pitch"), NanNew(espeak_GetParameter(espeakPITCH, 1)));
	obj->Set(NanNew("rate"), NanNew(espeak_GetParameter(espeakRATE, 1)));
	obj->Set(NanNew("volume"), NanNew(espeak_GetParameter(espeakVOLUME, 1)));
	obj->Set(NanNew("gap"), NanNew(espeak_GetParameter(espeakWORDGAP, 1)));
	obj->Set(NanNew("range"), NanNew(espeak_GetParameter(espeakRANGE, 1)));
	NanReturnValue(obj);
}

static NAN_METHOD(SetGender) {
	NanScope();
	double num = args[0]->NumberValue();
	gender = (char)num;
	reloadVoice();
	NanReturnUndefined();
}

static NAN_METHOD(SetAge) {
	NanScope();
	double num = args[0]->NumberValue();
	age = (char)num;
	reloadVoice();
	NanReturnUndefined();
}

static NAN_METHOD(SetVariant) {
	NanScope();
	double num = args[0]->NumberValue();
	variant = (char)num;
	reloadVoice();
	NanReturnUndefined();
}

static NAN_METHOD(SetRate) {
	NanScope();
	double num = args[0]->NumberValue();
	if(num < 80 || num > 450) {
		NanThrowTypeError("Invalid rate. Valid values are integers in range 80 to 450.");
	}
	else {
		espeak_SetParameter(espeakRATE, (int) num, 0);
	}
	NanReturnUndefined();
}

static NAN_METHOD(SetVolume) {
	NanScope();
	double num = args[0]->NumberValue();
	if(num < 0) {
		NanThrowTypeError("Invalid volume. Valid values are integers greater than 0. Integer bigger than 200 are not recommended.");
	}
	else {
		espeak_SetParameter(espeakVOLUME, (int) num, 0);
	}
	NanReturnUndefined();
}

static NAN_METHOD(SetPitch) {
	NanScope();
	double num = args[0]->NumberValue();
	if(num < 0 || num > 100) {
		NanThrowTypeError("Invalid pitch. Valid values are integer between 0 and 100. Default is 50.");
	}
	else {
		espeak_SetParameter(espeakPITCH, (int) num, 0);
	}
	NanReturnUndefined();
}

static NAN_METHOD(SetRange) {
	NanScope();
	double num = args[0]->NumberValue();
	if(num < 0 || num > 100) {
		NanThrowTypeError("Invalid range. Valid values are integers between 0 and 100. Default is 50.");
	}
	else {
		espeak_SetParameter(espeakRANGE, (int) num, 0);
	}
	NanReturnUndefined();
}

static NAN_METHOD(SetGap) {
	NanScope();
	double num = args[0]->NumberValue();
	if(num < 0) {
		NanThrowTypeError("Invalid gap. Valid values are integers greater than 0.");
	}
	else {
		espeak_SetParameter(espeakWORDGAP, (int) num, 0);
	}
	NanReturnUndefined();
}

static NAN_METHOD(SetLanguage) {
	NanScope();
	NanUtf8String str(args[0]);
	language = new char[strlen(*str) + 1];
	strcpy(language, *str);
	reloadVoice();
	NanReturnUndefined();
}

static NAN_METHOD(Initialize) {
	NanScope();
	flags = espeakCHARS_AUTO | espeakENDPAUSE;
	hasInit = true;
	if(!args[0]->IsUndefined()) {
		NanAsciiString path(args[0]);
		espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, *path, 0);
	}
	else {
		espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, NULL, 0);
	}
	espeak_SetSynthCallback(SynthCallback);
	espeak_SetVoiceByName("mb-lt1");
	NanReturnUndefined();
}

static NAN_METHOD(Speak) {
	NanScope();
	if(!hasInit) {
		NanThrowError("ESpeak was not initialized but speak() was called. Did you forget to call initialize()?");
	}
	else {
		NanUtf8String str(args[0]);
		char *text = *str;
		espeak_Synth(text, str.length(), 0,POS_CHARACTER,0, flags, NULL, NULL);
		espeak_Synchronize();
	}
	NanReturnUndefined();
}

static NAN_METHOD(OnVoice) {
	NanScope();
	callback = new NanCallback(args[0].As<Function>());
	hasCallback = true;
	NanReturnUndefined();
}

static void InitESpeak(Handle<Object> exports) {
	exports->Set(NanNew("initialize"), NanNew<FunctionTemplate>(Initialize)->GetFunction());
	exports->Set(NanNew("speak"), NanNew<FunctionTemplate>(Speak)->GetFunction());
	exports->Set(NanNew("onVoice"), NanNew<FunctionTemplate>(OnVoice)->GetFunction());

	exports->Set(NanNew("setAge"), NanNew<FunctionTemplate>(SetAge)->GetFunction());
	exports->Set(NanNew("setGender"), NanNew<FunctionTemplate>(SetGender)->GetFunction());
	exports->Set(NanNew("setLanguage"), NanNew<FunctionTemplate>(SetLanguage)->GetFunction());
	exports->Set(NanNew("setVariant"), NanNew<FunctionTemplate>(SetVariant)->GetFunction());

	exports->Set(NanNew("setPitch"), NanNew<FunctionTemplate>(SetPitch)->GetFunction());
	exports->Set(NanNew("setRange"), NanNew<FunctionTemplate>(SetRange)->GetFunction());
	exports->Set(NanNew("setRate"), NanNew<FunctionTemplate>(SetRate)->GetFunction());
	exports->Set(NanNew("setVolume"), NanNew<FunctionTemplate>(SetVolume)->GetFunction());
	exports->Set(NanNew("setGap"), NanNew<FunctionTemplate>(SetGap)->GetFunction());

	exports->Set(NanNew("getProperties"), NanNew<FunctionTemplate>(GetProperties)->GetFunction());
	exports->Set(NanNew("getVoice"), NanNew<FunctionTemplate>(GetVoice)->GetFunction());
}

NODE_MODULE(node_espeak, InitESpeak);
