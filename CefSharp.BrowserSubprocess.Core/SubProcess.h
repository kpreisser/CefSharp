// Copyright © 2016 The CefSharp Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "Stdafx.h"
#include "include/cef_app.h"

#include "SubProcessApp.h"
#include "CefBrowserWrapper.h"
#include "CefAppUnmanagedWrapper.h"

using namespace System::Collections::Generic;
using namespace System::Linq;
using namespace CefSharp::Internals;
using namespace CefSharp::RenderProcess;

namespace CefSharp
{
    namespace BrowserSubprocess
    {
        // Wrap CefAppUnmangedWrapper in a nice managed wrapper
        public ref class SubProcess
        {
        private:
            MCefRefPtr<CefAppUnmanagedWrapper> _cefApp;

            static void AwaitParentProcessExit(Object^ boxedParentProcessId)
            {
                int parentProcessId = (int)boxedParentProcessId;
                try
                {
                    auto parentProcess = Process::GetProcessById(parentProcessId);
                    parentProcess->WaitForExit();
                }
                catch (Exception ^ e)
                {
                    //main process probably died already
                    Debug::WriteLine(e);
                }

                //Thread::Sleep(1000); //wait a bit before exiting

                Debug::WriteLine("BrowserSubprocess shutting down forcibly.");

                Process::GetCurrentProcess()->Kill();
            }

            static int SubProcessMain(IEnumerable<String^>^ args) {
                Debug::WriteLine("BrowserSubprocess starting up with command line: " + String::Join("\n", args));

                SubProcess::EnableHighDPISupport();

                int result;
                auto type = CommandLineArgsParser::GetArgumentValue(args, CefSharpArguments::SubProcessTypeArgument);

                int parentProcessId = -1;

                // The Crashpad Handler doesn't have any HostProcessIdArgument, so we must not try to
                // parse it lest we want an ArgumentNullException.
                if (type != "crashpad-handler")
                {
                    parentProcessId = int::Parse(CommandLineArgsParser::GetArgumentValue(args, CefSharpArguments::HostProcessIdArgument));
                    if (CommandLineArgsParser::HasArgument(args, CefSharpArguments::ExitIfParentProcessClosed))
                    {
                        auto thread = gcnew System::Threading::Thread(
                            gcnew System::Threading::ParameterizedThreadStart(&SubProcess::AwaitParentProcessExit));
                        // Use a background thread so that it does not prevent the
                        // process from exiting if Main() did already return.
                        thread->IsBackground = true;
                        thread->Start(parentProcessId);
                    }
                }

                // Use our custom subProcess provides features like EvaluateJavascript
                if (type == "renderer")
                {
                    //Add your own custom implementation of IRenderProcessHandler here
                    IRenderProcessHandler^ handler = nullptr;
                    auto wcfEnabled = CommandLineArgsParser::HasArgument(args, CefSharpArguments::WcfEnabledArgument);
                    SubProcess^ subProcess; // = wcfEnabled ? (gcnew WcfEnabledSubProcess(parentProcessId, handler, args)) : (gcnew SubProcess(handler, args));
                    if (wcfEnabled) {
                        subProcess = gcnew WcfEnabledSubProcess(parentProcessId, handler, args);
                    }
                    else {
                        subProcess = gcnew SubProcess(handler, args);
                    }

                    try
                    {
                        result = subProcess->Run();
                    }
                    finally {
                        //subProcess->Dispose();
                    }
                }
                else
                {
                    result = SubProcess::ExecuteProcess(args);
                }

                Debug::WriteLine("BrowserSubprocess shutting down.");

                return result;
            }

        public:
            SubProcess(IRenderProcessHandler^ handler, IEnumerable<String^>^ args)
            {
                auto onBrowserCreated = gcnew Action<CefBrowserWrapper^>(this, &SubProcess::OnBrowserCreated);
                auto onBrowserDestroyed = gcnew Action<CefBrowserWrapper^>(this, &SubProcess::OnBrowserDestroyed);
                auto schemes = CefCustomScheme::ParseCommandLineArguments(args);
                auto enableFocusedNodeChanged = CommandLineArgsParser::HasArgument(args, CefSharpArguments::FocusedNodeChangedEnabledArgument);

                _cefApp = new CefAppUnmanagedWrapper(handler, schemes, enableFocusedNodeChanged, onBrowserCreated, onBrowserDestroyed);
            }

            !SubProcess()
            {
                _cefApp = nullptr;
            }

            ~SubProcess()
            {
                this->!SubProcess();
            }

            int Run()
            {
                auto hInstance = Process::GetCurrentProcess()->Handle;

                CefMainArgs cefMainArgs((HINSTANCE)hInstance.ToPointer());

                return CefExecuteProcess(cefMainArgs, (CefApp*)_cefApp.get(), NULL);
            }

            virtual void OnBrowserCreated(CefBrowserWrapper^ cefBrowserWrapper)
            {

            }

            virtual void OnBrowserDestroyed(CefBrowserWrapper^ cefBrowserWrapper)
            {

            }

            static void EnableHighDPISupport()
            {
                CefEnableHighDPISupport();
            }

            static int ExecuteProcess(IEnumerable<String^>^ args)
            {
                auto hInstance = Process::GetCurrentProcess()->Handle;

                CefMainArgs cefMainArgs((HINSTANCE)hInstance.ToPointer());

                auto schemes = CefCustomScheme::ParseCommandLineArguments(args);

                CefRefPtr<CefApp> app = new SubProcessApp(schemes);

                return CefExecuteProcess(cefMainArgs, app, NULL);
            }

            static void HandleAppStart() {
                // Check if the first argument starts with "--type=", in which case
                // we run the subprocess logic. Otherwise, we simply return and allow
                // the app run its own logic.
                auto args = Enumerable::Skip<String^>(Environment::GetCommandLineArgs(), 1);
                if (CommandLineArgsParser::HasArgument(args, CefSharpArguments::SubProcessTypeArgument + "="))
                {
                    // Run the subprocess.
                    int exitCode = SubProcessMain(args);

                    // Exit directly so that the remaining application logic is not run.
                    Environment::Exit(exitCode);
                }
            }
        };
    }
}
