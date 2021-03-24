#include <jni.h>
#include <string>
#include "AudioConverter.h"
#include <android/log.h>

static void log_callback(void* ptr, int level, const char* fmt, va_list vl) {
    va_list vl2;
    char line[1024];
    va_copy(vl2, vl);
    static int print_prefix = 1;
    av_log_format_line(ptr, level, fmt, vl2, line, 1024, &print_prefix);
    va_end(vl2);
    __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "%s", line);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_tech_smallwonder_audioconverter_AudioConverter_newAudioConverterNative(JNIEnv *env, jobject thiz, jstring url) {
    const char* url_str = env->GetStringUTFChars(url, JNI_FALSE);
    auto* audio_converter = new audio_convert::AudioConverter(url_str);

    av_log_set_callback(log_callback);

    return reinterpret_cast<jlong>(audio_converter);
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_tech_smallwonder_audioconverter_AudioConverter_initializeNative(JNIEnv *env, jobject thiz, jlong id) {
    auto* audio_converter = reinterpret_cast<audio_convert::AudioConverter*>(id);
    auto initialized = audio_converter->initialize();
    if (initialized) {
        __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Initialized audio converter native!");
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Unable to init audio converter native!");
    }
    return static_cast<jboolean>(audio_converter->is_initialized() ? JNI_TRUE : JNI_FALSE);
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_tech_smallwonder_audioconverter_AudioConverter_convertNative(JNIEnv *env, jobject thiz, jlong id, jint output_format, jstring output_path, jobjectArray metadata_keys, jobjectArray metadata_values) {
    auto* audio_converter = reinterpret_cast<audio_convert::AudioConverter*>(id);
    std::map<std::string, std::string> metadata_map{};
    std::string o_path = env->GetStringUTFChars(output_path, JNI_FALSE);
    auto length = env->GetArrayLength(metadata_keys);
    for (int i = 0; i < length; i++) {
        std::string key = env->GetStringUTFChars(
                static_cast<jstring>(env->GetObjectArrayElement(metadata_keys, i)), JNI_FALSE);
        std::string value = env->GetStringUTFChars(
                static_cast<jstring>(env->GetObjectArrayElement(metadata_values, i)), JNI_FALSE);

        metadata_map[key] = value;
    }
    return static_cast<jboolean>(audio_converter->convert((AVCodecID) output_format, o_path, metadata_map));
}
extern "C"
JNIEXPORT jint JNICALL
Java_tech_smallwonder_audioconverter_AudioConverter_getConversionProgress(JNIEnv *env, jobject thiz, jlong id) {
    auto* audio_converter = reinterpret_cast<audio_convert::AudioConverter*>(id);
    return audio_converter->get_percentage();
}
extern "C"
JNIEXPORT void JNICALL
Java_tech_smallwonder_audioconverter_AudioConverter_releaseNative(JNIEnv *env, jobject thiz,
                                                                  jlong id) {
    auto* audio_converter = reinterpret_cast<audio_convert::AudioConverter*>(id);
    delete audio_converter;
}