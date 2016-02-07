/*
 * Copyright (c) 2015 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APRINTER_WEB_INTERFACE_MODULE_H
#define APRINTER_WEB_INTERFACE_MODULE_H

#include <stddef.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/net/http/HttpServer.h>
#include <aprinter/fs/BufferedFile.h>

#define APRINTER_ENABLE_HTTP_TEST 1

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class WebInterfaceModule {
public:
    struct Object;
    
private:
    using TheHttpServerService = HttpServerService<
        typename Params::HttpServerNetParams,
        128,   // MaxRequestLineLength
        128,   // MaxHeaderLineLength
        250,   // ExpectedResponseLength
        10000, // MaxRequestHeadLength
        4      // TxChunkHeaderDigits
    >;
    
    struct HttpRequestHandler;
    struct UserClientState;
    using TheHttpServer = typename TheHttpServerService::template Server<Context, Object, ThePrinterMain, HttpRequestHandler, UserClientState>;
    using TheRequestInterface = typename TheHttpServer::TheRequestInterface;
    
    using TheFsAccess = typename ThePrinterMain::template GetFsAccess<>;
    using TheBufferedFile = BufferedFile<Context, TheFsAccess>;
    
    static size_t const GetSdChunkSize = 512;
    static_assert(GetSdChunkSize <= TheHttpServer::MaxTxChunkSize, "");
    
    static constexpr char const * WebRootPath() { return "www"; }
    static constexpr char const * IndexPage() { return "reprap.htm"; }
    
public:
    static void init (Context c)
    {
        TheHttpServer::init(c);
    }
    
    static void deinit (Context c)
    {
        TheHttpServer::deinit(c);
    }
    
private:
    static char const * get_content_type (char const *path)
    {
        size_t path_len = strlen(path);
        
        if (AsciiCaseInsensEndsWith(path, path_len, ".htm") || AsciiCaseInsensEndsWith(path, path_len, ".html")) {
            return "text/html";
        }
        if (AsciiCaseInsensEndsWith(path, path_len, ".css")) {
            return "text/css";
        }
        if (AsciiCaseInsensEndsWith(path, path_len, ".js")) {
            return "application/javascript";
        }
        if (AsciiCaseInsensEndsWith(path, path_len, ".png")) {
            return "image/png";
        }
        if (AsciiCaseInsensEndsWith(path, path_len, ".ico")) {
            return "image/x-icon";
        }
        
        return "application/octet-stream";
    }
    
    static void http_request_handler (Context c, TheRequestInterface *request)
    {
        char const *method = request->getMethod(c);
        char const *path = request->getPath(c);
        
        if (!strcmp(method, "GET")) {
            if (request->hasRequestBody(c)) {
                request->setResponseStatus(c, HttpStatusCodes::BadRequest());
                goto error;
            }
#if APRINTER_ENABLE_HTTP_TEST
            if (!strcmp(path, "/downloadTest")) {
                return request->getUserClientState(c)->acceptDownloadTestRequest(c, request);
            }
#endif
            if (path[0] != '/') {
                request->setResponseStatus(c, HttpStatusCodes::NotFound());
                goto error;
            }
            char const *file_path = !strcmp(path, "/") ? IndexPage() : (path + 1);
            return request->getUserClientState(c)->acceptGetFileRequest(c, request, file_path);
        }
        else if (!strcmp(method, "POST")) {
            if (!request->hasRequestBody(c)) {
                request->setResponseStatus(c, HttpStatusCodes::BadRequest());
                goto error;
            }
#if APRINTER_ENABLE_HTTP_TEST
            if (!strcmp(path, "/uploadTest")) {
                return request->getUserClientState(c)->acceptUploadTestRequest(c, request);
            }
#endif
            request->setResponseStatus(c, HttpStatusCodes::NotFound());
        }
        else {
            request->setResponseStatus(c, HttpStatusCodes::MethodNotAllowed());
        }
        
    error:
        request->completeHandling(c);
    }
    struct HttpRequestHandler : public AMBRO_WFUNC_TD(&WebInterfaceModule::http_request_handler) {};
    
    struct UserClientState : public TheRequestInterface::RequestUserCallback {
        enum class State {NO_CLIENT, OPEN, WAIT, READ, DL_TEST, UL_TEST};
        
        void init (Context c)
        {
            m_buffered_file.init(c, APRINTER_CB_OBJFUNC_T(&UserClientState::buffered_file_handler, this));
            m_state = State::NO_CLIENT;
        }
        
        void deinit (Context c)
        {
            m_buffered_file.deinit(c);
        }
        
        void acceptGetFileRequest (Context c, TheRequestInterface *request, char const *file_path)
        {
            AMBRO_ASSERT(m_state == State::NO_CLIENT)
            
            request->setCallback(c, this);
            m_request = request;
            m_file_path = file_path;
            m_state = State::OPEN;
            m_buffered_file.startOpen(c, file_path, false, TheBufferedFile::OpenMode::OPEN_READ, WebRootPath());
        }
        
#if APRINTER_ENABLE_HTTP_TEST
        void acceptDownloadTestRequest (Context c, TheRequestInterface *request)
        {
            AMBRO_ASSERT(m_state == State::NO_CLIENT)
            
            request->setCallback(c, this);
            m_request = request;
            m_request->setResponseContentType(c, "application/octet-stream");
            m_request->adoptResponseBody(c);
            m_state = State::DL_TEST;
        }
        
        void acceptUploadTestRequest (Context c, TheRequestInterface *request)
        {
            AMBRO_ASSERT(m_state == State::NO_CLIENT)
            
            request->setCallback(c, this);
            m_request = request;
            m_request->adoptRequestBody(c);
            m_state = State::UL_TEST;
        }
#endif
        
        void requestTerminated (Context c)
        {
            AMBRO_ASSERT(m_state != State::NO_CLIENT)
            
            m_buffered_file.reset(c);
            m_state = State::NO_CLIENT;
        }
        
#if APRINTER_ENABLE_HTTP_TEST
        void requestBufferEvent (Context c)
        {
            AMBRO_ASSERT(m_state == State::UL_TEST)
            
            auto buf_st = m_request->getRequestBodyBufferState(c);
            if (buf_st.length > 0) {
                m_request->acceptRequestBodyData(c, buf_st.length);
            }
            else if (buf_st.eof) {
                m_request->completeHandling(c);
                requestTerminated(c);
            }
        }
#endif
        
        void responseBufferEvent (Context c)
        {
            switch (m_state) {
                case State::WAIT: {
                    auto buf_st = m_request->getResponseBodyBufferState(c);
                    size_t allowed_length = MinValue(GetSdChunkSize, buf_st.length);
                    if (m_cur_chunk_size < allowed_length) {
                        auto dest_buf = buf_st.data.subFrom(m_cur_chunk_size);
                        m_buffered_file.startReadData(c, dest_buf.ptr1, MinValue(dest_buf.wrap, (size_t)(allowed_length - m_cur_chunk_size)));
                        m_state = State::READ;
                    }
                } break;
                
                case State::READ:
                    break;
                
#if APRINTER_ENABLE_HTTP_TEST
                case State::DL_TEST: {
                    while (true) {
                        auto buf_st = m_request->getResponseBodyBufferState(c);
                        if (buf_st.length < GetSdChunkSize) {
                            break;
                        }
                        size_t len1 = MinValue(GetSdChunkSize, buf_st.data.wrap);
                        memset(buf_st.data.ptr1, 'X', len1);
                        if (len1 < GetSdChunkSize) {
                            memset(buf_st.data.ptr2, 'X', GetSdChunkSize-len1);
                        }
                        m_request->provideResponseBodyData(c, GetSdChunkSize);
                    }
                } break;
#endif
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        void buffered_file_handler (Context c, typename TheBufferedFile::Error error, size_t read_length)
        {
            switch (m_state) {
                case State::OPEN: {
                    if (error != TheBufferedFile::Error::NO_ERROR) {
                        auto status = (error == TheBufferedFile::Error::NOT_FOUND) ? HttpStatusCodes::NotFound() : HttpStatusCodes::InternalServerError();
                        m_request->setResponseStatus(c, status);
                        m_request->completeHandling(c);
                        return requestTerminated(c);
                    }
                    
                    m_request->setResponseContentType(c, get_content_type(m_file_path));
                    m_request->adoptResponseBody(c);
                    
                    m_state = State::WAIT;
                    m_cur_chunk_size = 0;
                } break;
                
                case State::READ: {
                    if (error != TheBufferedFile::Error::NO_ERROR) {
                        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpSdReadError\n"));
                        m_request->completeHandling(c);
                        return requestTerminated(c);
                    }
                    
                    AMBRO_ASSERT(read_length <= GetSdChunkSize - m_cur_chunk_size)
                    m_cur_chunk_size += read_length;
                    
                    if (m_cur_chunk_size == GetSdChunkSize || (read_length == 0 && m_cur_chunk_size > 0)) {
                        m_request->provideResponseBodyData(c, m_cur_chunk_size);
                        m_cur_chunk_size = 0;
                    }
                    
                    if (read_length == 0) {
                        m_request->completeHandling(c);
                        return requestTerminated(c);
                    }
                    
                    m_state = State::WAIT;
                    responseBufferEvent(c);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        TheBufferedFile m_buffered_file;
        TheRequestInterface *m_request;
        char const *m_file_path;
        size_t m_cur_chunk_size;
        State m_state;
    };
    
public:
    struct Object : public ObjBase<WebInterfaceModule, ParentObject, MakeTypeList<
        TheHttpServer
    >> {};
};

APRINTER_ALIAS_STRUCT_EXT(WebInterfaceModuleService, (
    APRINTER_AS_TYPE(HttpServerNetParams)
), (
    APRINTER_MODULE_TEMPLATE(WebInterfaceModuleService, WebInterfaceModule)
))

#include <aprinter/EndNamespace.h>

#endif
