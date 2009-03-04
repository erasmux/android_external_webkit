/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * All rights reserved.
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
#include "FileSystem.h"

#include "CString.h"
#include <dlfcn.h>
#include <errno.h>
#include <sys/stat.h>
#include "cutils/log.h"

namespace WebCore {

// Global static used to store the base to the plugin path.
// This is set in WebSettings.cpp
String sPluginPath;

CString fileSystemRepresentation(const String& path) {
    return path.utf8();
}

CString openTemporaryFile(const char* prefix, PlatformFileHandle& handle)
{
    int number = rand() % 10000 + 1;
    CString filename;
    do {
        String path = sPluginPath;
        path.append("/");
        path.append(prefix);
        path.append(String::number(number));
        filename = path.utf8();
        const char *fstr = filename.data();
        handle = open(filename.data(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        number++;
    } while (handle == -1 && errno == EEXIST);
    
    if (handle != -1) {
        return filename;
    }
    return CString();
}

bool unloadModule(PlatformModule module)
{
    return dlclose(module) == 0;
}

void closeFile(PlatformFileHandle& handle)
{
    if (isHandleValid(handle)) {
        close(handle);
        handle = invalidPlatformFileHandle;
    }
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    int totalBytesWritten = 0;
    while (totalBytesWritten < length) {
        int bytesWritten = write(handle, data, length - totalBytesWritten);
        if (bytesWritten < 0 && errno != EINTR)
            return -1;
        else if (bytesWritten > 0)
            totalBytesWritten += bytesWritten;
    }

    return totalBytesWritten;
}

// new as of SVN change 36269, Sept 8, 2008
String homeDirectoryPath() 
{
    return sPluginPath;
}

}