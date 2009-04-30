/*
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "jni_instance.h"
#include "jni_runtime.h"
#include "jni_utility.h"

#include <assert.h>

#ifdef NDEBUG
#define JS_LOG(formatAndArgs...) ((void)0)
#else
#define JS_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:%d -- %s:  ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}
#endif
 
using namespace JSC::Bindings;

JavaInstance::JavaInstance (jobject instance)
{
    _instance = new JObjectWrapper (instance);
}

JavaInstance::~JavaInstance () 
{
}

#define NUM_LOCAL_REFS 64

bool JavaInstance::invokeMethod(NPIdentifier methodName, NPVariant* args, uint32_t count, NPVariant* resultValue)
{
    int i;
    jvalue *jArgs;
    Method *method = 0;
    size_t numMethods = methodList.size();
    
    // Try to find a good match for the overloaded method.  The 
    // fundamental problem is that JavaScript doesn have the
    // notion of method overloading and Java does.  We could 
    // get a bit more sophisticated and attempt to does some
    // type checking as we as checking the number of parameters.
    Method *aMethod;
    for (size_t methodIndex = 0; methodIndex < numMethods; methodIndex++) {
        aMethod = methodList[methodIndex];
        if (aMethod->numParameters() == count) {
            method = aMethod;
            break;
        }
    }
    if (method == 0) {
        JS_LOG ("unable to find an appropiate method\n");
        return jsUndefined();
    }
    
    const JavaMethod *jMethod = static_cast<const JavaMethod*>(method);
    
    if (count > 0) {
        jArgs = (jvalue *)malloc (count * sizeof(jvalue));
    }
    else
        jArgs = 0;
        
    for (i = 0; i < count; i++) {
        JavaParameter* aParameter = jMethod->parameterAt(i);
        jArgs[i] = convertNPVariantToJValue(args[i], aParameter->getJNIType(), aParameter->type());
    }
        
    jvalue result;

    // The following code can be conditionally removed once we have a Tiger update that
    // contains the new Java plugin.  It is needed for builds prior to Tiger.
    {    
        jobject obj = _instance->_instance;
        switch (jMethod->JNIReturnType()){
            case void_type:
                callJNIMethodIDA<void>(obj, jMethod->methodID(obj), jArgs);
                break;            
            case object_type:
                result.l = callJNIMethodIDA<jobject>(obj, jMethod->methodID(obj), jArgs);
                break;
            case boolean_type:
                result.z = callJNIMethodIDA<jboolean>(obj, jMethod->methodID(obj), jArgs);
                break;
            case byte_type:
                result.b = callJNIMethodIDA<jbyte>(obj, jMethod->methodID(obj), jArgs);
                break;
            case char_type:
                result.c = callJNIMethodIDA<jchar>(obj, jMethod->methodID(obj), jArgs);
                break;            
            case short_type:
                result.s = callJNIMethodIDA<jshort>(obj, jMethod->methodID(obj), jArgs);
                break;
            case int_type:
                result.i = callJNIMethodIDA<jint>(obj, jMethod->methodID(obj), jArgs);
                break;
            
            case long_type:
                result.j = callJNIMethodIDA<jlong>(obj, jMethod->methodID(obj), jArgs);
                break;
            case float_type:
                result.f = callJNIMethodIDA<jfloat>(obj, jMethod->methodID(obj), jArgs);
                break;
            case double_type:
                result.d = callJNIMethodIDA<jdouble>(obj, jMethod->methodID(obj), jArgs);
                break;
            case invalid_type:
            default:
                break;
        }
    }
        
    switch (jMethod->JNIReturnType()){
        case void_type: {
            VOID_TO_NPVARIANT(*resultValue);
        }
        break;
        
        case object_type: {
            if (result.l != 0) {
                OBJECT_TO_NPVARIANT(JavaObjectToNPObject(JavaInstance::create(result.l)), *resultValue);
            }
            else {
                VOID_TO_NPVARIANT(*resultValue);
            }
        }
        break;
        
        case boolean_type: {
            BOOLEAN_TO_NPVARIANT(result.z, *resultValue);
        }
        break;
        
        case byte_type: {
            INT32_TO_NPVARIANT(result.b, *resultValue);
        }
        break;
        
        case char_type: {
            INT32_TO_NPVARIANT(result.c, *resultValue);
        }
        break;
        
        case short_type: {
            INT32_TO_NPVARIANT(result.s, *resultValue);
        }
        break;
        
        case int_type: {
            INT32_TO_NPVARIANT(result.i, *resultValue);
        }
        break;
       
        // TODO(fqian): check if cast to double is needed.
        case long_type: {
            DOUBLE_TO_NPVARIANT(result.j, *resultValue);
        }
        break;
        
        case float_type: {
            DOUBLE_TO_NPVARIANT(result.f, *resultValue);
        }
        break;
        
        case double_type: {
            DOUBLE_TO_NPVARIANT(result.d, *resultValue);
        }
        break;

        case invalid_type:
        default: {
            VOID_TO_NPVARIANT(*resultValue);
        }
        break;
    }

    free (jArgs);

    return resultValue;
}

JObjectWrapper::JObjectWrapper(jobject instance)
: _refCount(0)
{
    assert (instance != 0);

    // Cache the JNIEnv used to get the global ref for this java instanace.
    // It'll be used to delete the reference.
    _env = getJNIEnv();
        
    _instance = _env->NewGlobalRef (instance);
    
    JS_LOG ("new global ref %p for %p\n", _instance, instance);

    if  (_instance == NULL) {
        fprintf (stderr, "%s:  could not get GlobalRef for %p\n", __PRETTY_FUNCTION__, instance);
    }
}

JObjectWrapper::~JObjectWrapper() {
    JS_LOG ("deleting global ref %p\n", _instance);
    _env->DeleteGlobalRef (_instance);
}