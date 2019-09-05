// Copyright © 2013 The CefSharp Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

namespace CefSharp.BrowserSubprocess
{
    /// <summary>
    /// When implementing your own BrowserSubprocess
    /// - Include an app.manifest with the dpi/compatability sections, this is required (this project contains the relevant).
    /// - If you are targeting x86/Win32 then you should set /LargeAddressAware (https://docs.microsoft.com/en-us/cpp/build/reference/largeaddressaware?view=vs-2017)
    /// </summary>
    class Program
    {
        static void Main()
        {
            SubProcess.HandleAppStart();
        }
    }
}
