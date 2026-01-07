# LogViewer License

## Application License

**Proprietary License**

Copyright (c) 2022-2026 LogViewer Contributors

All rights reserved.

### Terms and Conditions

**Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to:**

1. **Use** the Software for personal, educational, or commercial purposes
2. **Copy** and distribute verbatim copies of the Software
3. **Modify** the Software only with explicit written permission from the copyright holders

**The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.**

### Restrictions

**The following activities are expressly prohibited without prior written permission from the copyright holders:**

1. Modification, adaptation, or creation of derivative works based on the Software
2. Reverse engineering, decompilation, or disassembly of the Software
3. Removal or alteration of copyright notices
4. Commercial redistribution without explicit licensing agreement

### Warranty Disclaimer

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### Contact

For permission to modify or commercially distribute this software, please contact the LogViewer Development Team at: https://github.com/bula173/LogViewer/issues

---

## Third-Party Libraries and Licenses

### Qt Framework

This application uses Qt 6, which is available under multiple licenses:

**Open Source License:**
- Qt is licensed under the GNU Lesser General Public License (LGPL) version 3
- This application dynamically links to Qt libraries, complying with LGPL requirements
- Source code for Qt is available at: https://www.qt.io/download-open-source

**Commercial License:**
- For commercial use that requires features beyond LGPL terms, obtain a commercial Qt license
- See: https://www.qt.io/licensing/

**LGPL v3 Compliance for this Application:**
- Qt libraries are dynamically linked (not statically linked)
- Users can replace Qt libraries with compatible versions
- No modifications have been made to Qt source code
- Full Qt source code is available from The Qt Company

For the complete LGPL v3 license text, see: https://www.gnu.org/licenses/lgpl-3.0.html

### Other Third-Party Libraries

#### nlohmann/json
- License: MIT License
- Copyright (c) 2013-2022 Niels Lohmann
- Source: https://github.com/nlohmann/json

#### spdlog
- License: MIT License
- Copyright (c) 2016 Gabi Melman
- Source: https://github.com/gabime/spdlog

#### Expat XML Parser
- License: MIT License
- Copyright (c) 1998-2000 Thai Open Source Software Center Ltd and Clark Cooper
- Copyright (c) 2001-2022 Expat maintainers
- Source: https://github.com/libexpat/libexpat

#### Google Test (gtest)
- License: BSD 3-Clause License
- Copyright 2008, Google Inc.
- Source: https://github.com/google/googletest

#### gflags
- License: BSD 3-Clause License
- Copyright (c) 2006, Google Inc.
- Source: https://github.com/gflags/gflags

#### CURL
- License: curl License (MIT/X derivative)
- Copyright (c) 1996 - 2024, Daniel Stenberg
- Source: https://curl.se/

---

## AI Provider Services

This application can connect to third-party AI services. Use of these services is subject to their respective terms of service and pricing:

- **Ollama**: Open source, locally hosted (Apache 2.0)
- **LM Studio**: Free for personal use
- **OpenAI**: Commercial service with API pricing
- **Anthropic (Claude)**: Commercial service with API pricing
- **Google Gemini**: Commercial service with API pricing
- **xAI (Grok)**: Commercial service with API pricing

Users are responsible for obtaining appropriate API keys and complying with each provider's terms of service.

---

## Distribution Notes

### For Binary Distributions

When distributing compiled binaries of LogViewer:

1. **Qt LGPL Compliance:**
   - Include this license file
   - Provide clear information that the application uses Qt under LGPL v3
   - Inform users they can obtain Qt source code from https://www.qt.io/
   - Ensure Qt libraries are dynamically linked (not statically linked)
   - Provide a way for users to replace Qt libraries if needed

2. **Other Libraries:**
   - Include license notices for all third-party libraries
   - Most are MIT-licensed and only require attribution

3. **AI Services:**
   - Inform users that AI features require external services
   - Direct users to obtain their own API keys
   - Link to each provider's terms of service

### For Source Code Distributions

- The full source code satisfies LGPL requirements
- All third-party library source code is available from their respective repositories
- Build instructions are provided in `docs/DEVELOPMENT.md`

---

## Additional Information

For questions about licensing, please open an issue at:
https://github.com/bula173/LogViewer/issues

For Qt licensing questions, visit:
https://www.qt.io/licensing/
