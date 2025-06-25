# PrintHtml

PrintHtml is a command line program to print HTML pages and more importantly allows you to
send the output to a specific printer. I was searching for a long time to find a solution
to this problem after we ran into more and more issues using the .net WebBrowser control.
Most recently the WebBrowser control kept failing and silently not printing until we
rebooted the computer and updating to Windows 10 made no difference. It worked flawlessly
for probably 4 years, but now it seems it has problems. On top of that we needed the
ability to print to different printers and the WebBrowser control always sends the output
to whatever printer is configured as the default for Internet Explorer.

Many years ago we wrote a version of our printing code for packing slips using C++ and
the QtWebKit library. But when we ported all our code to C# and .net, we moved to using the
.net WebBrowser control and it served us faithfully for many years. So to solve the problems
we had with the .net WebBrowser control I dug up the old C++ application that printed web pages
files and turned it into this project to print via the command line, and enabled it to print
to different printers.

The program is pretty simple and the command line usage is like this:

~~~~
Usage: PrintHtml [mode_options] [global_options]

Global Options:
  -server [port]        - Run as REST server on given port (default 8080).
                          When in server mode, specific print or scan operations are triggered via HTTP requests.
  -json                 - Output success and error information as JSON to stdout (no message boxes).
                          This applies to command-line print or scan operations.

The application operates in one of two modes: Print Mode or Scan Mode. These modes are mutually exclusive.

---

### Print Mode (Default)

Prints HTML pages from URLs. This is the default mode if `--scan` is not specified.

Command Line Options for Print Mode:
  <url> [url2...]       - One or more URLs to print (space-separated). Required if not in server mode.
  -test                 - Don't print, just show what would have printed.
  -p printer            - Printer to print to. Use 'Default' for default printer.
  -l left               - Optional left margin (default: 0.5 inches).
  -t top                - Optional top margin (default: 0.5 inches).
  -r right              - Optional right margin (default: 0.5 inches).
  -b bottom             - Optional bottom margin (default: 0.5 inches).
  -a paper              - Paper type. Options:
                          • Standard sizes: [A4|A5|Letter]
                          • Custom size: width,height in millimeters (e.g., 77,77)
  -o [Portrait|Landscape] - Optional page orientation (default: Portrait).
  -pagefrom [number]    - Optional. Start page number for printing range.
  -pageto [number]      - Optional. End page number for printing range. (Must be used with -pagefrom)

Example (Print HTML with custom paper size 77x77 mm, no margins):
  PrintHtml -p "YourPrinterName" -a "77,77" -l 0 -r 0 -t 0 -b 0 "https://example.com"

---

### Scan Mode

Scans a document from a connected scanner device. The output can be saved to a local file and/or uploaded to a specified URL.

Command Line Options for Scan Mode:
  --scan                - Activates scan mode.
  --scanner <name>      - Specifies the scanner device to use (e.g., "MyScanner").
                          Use "Default" for the system's default scanner. (Default: "Default")
  --output-file <path>  - Specifies the full file path to save the scanned image (e.g., "C:/scans/image.png").
                          If not provided, the image is not saved locally.
  --upload-url <url>    - Specifies a URL to which the scanned image will be uploaded as a POST request (form-data, field name "image").
                          If not provided, the image is not uploaded.

Example (Scan from default scanner, save locally, and upload):
  PrintHtml --scan --output-file "scan_result.png" --upload-url "http://example.com/upload_scan"

Note: You must specify at least `--output-file` or `--upload-url` in scan mode for the scanned image to be processed.

---

REST server example (listen on port 9090):
  PrintHtml -server 9090

Once the server is running, you can send HTTP GET requests to the following endpoints.
**Note:** Print and Scan operations are mutually exclusive. The server handles one type of request at a time.

#### Print Endpoint (`/print`)

Parameters for `/print`:
  url                   - Page URL to print (required).
  p                     - Printer name.
  a                     - Paper size (e.g., A4, Letter, or custom WIDTH,HEIGHT in mm).
  l, t, r, b            - Margins (in inches, optional).
  o                     - Orientation (Portrait or Landscape).
  pagefrom, pageto      - Print range.

Example (Print via REST with custom size):
  `http://localhost:9090/print?url=https://example.com&a=77,77`

#### Scan Endpoint (`/scan`)

Parameters for `/scan`:
  scanner               - Name of the scanner to use (e.g., "Default", "MyScanner"). (Optional, default: "Default")
  output_file           - Full local path to save the scanned image. (Optional)
                          Example: `C%3A%2Fscans%2Fimage.png` (URL encoded path)
  upload_url            - URL to upload the scanned image to. (Optional)
                          Example: `http%3A%2F%2Fexample.com%2Fupload` (URL encoded)

At least one of `output_file` or `upload_url` should be provided for a meaningful operation.

Example (Scan via REST, save to a file, and upload to a URL):
  `http://localhost:9090/scan?scanner=MyScanner&output_file=C%3A%2Fscans%2Fscan.png&upload_url=http%3A%2F%2Fmyserver.com%2Fuploader`

The server will respond with a `202 Accepted` and initiate the scan. The detailed outcome of the scan operation (success/failure, file paths/upload status) will be logged by the server application. If the server was started with the `-json` flag, this detailed outcome will also be printed as a JSON string to the server's standard output. This means the HTTP client receives an acknowledgment, and the full status is available on the server side.

~~~~

---

## 🛠️ Building from Source

To compile this project on Windows, follow these steps:

### 1. Download the required MinGW toolchain

Download and extract the following toolchain (compatible with Qt 4.8.6):

🔗 [i686-4.8.2-release-posix-dwarf-rt\_v3-rev3.7z](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/4.8.2/threads-posix/dwarf/i686-4.8.2-release-posix-dwarf-rt_v3-rev3.7z)

📂 Extract it to a directory like:

```
E:/mingw32
```

---

### 2. Install Qt 4.8.6

Download and install Qt 4.8.6 built for MinGW 4.8.2:

🔗 [qt-opensource-windows-x86-mingw482-4.8.6-1.exe](https://download.qt.io/archive/qt/4.8/4.8.6/qt-opensource-windows-x86-mingw482-4.8.6-1.exe)

Make sure to install it to a path without spaces, such as:

```
E:/Qt/4.8.6
```

---

### 3. Build the Project

Open **Qt 4.8.6 Command Prompt**, then navigate to the project folder:

```sh
cd E:\PrintHtml-master\PrintHtml-master
```

Run the following commands:

```sh
qmake
mingw32-make release
```

After a successful build, the executable will be found in the `release` directory.


## 🔽 Precompiled Version (Download and run directly)

If you just want to use the tool without modifying or compiling the source code:

📦 A precompiled executable is available in:

```
release\release.zip
```

Simply extract it and run `PrintHtml.exe` as needed — no setup or installation required.

---



Since it has to spawn up an entire instance of the QtWebKit control in order to perform the printing
the program is written to accept multiple URL's on the command line, one after the other. So if you have
large batches of URL's to print, like we do simply pass them all on the command line. In our case we
print our pick sheets using this tool by passing in batches of 20 URL's at a time and it works very fast
without anything showing on the screen.

# Caveats

The biggest caveat at the moment is that the QtWebKit control has no support for renderin headers and
footers on the page. For us this is not a big problem, but for some it could be a deal breaker. There have
been quite a few discussion about this that I could find on the web, but no solutions that seemed to work.
If anyone has ideas about how to fix this it would be great to add some options to control the headers
and footers to this program.

# REST server mode

PrintHtml can also run as a lightweight REST service. Start the application with
`-server [port]` (default `8080`) and it will listen for HTTP requests. Send a
`GET /print` request using query parameters that match the command line options
in their short forms (`p`, `l`, `t`, `r`, `b`, `o`, `a`). The server accepts 
either style of parameter and always returns a JSON response. Custom paper 
sizes can be provided using `width` and `height` query parameters or the 
shorthand `a=WIDTH,HEIGHT`.


<h4>🧾 REST Parameters</h4>

Parameter | Description
-- | --
url | Page URL to print (required)
p | Printer name
a | Paper size (e.g., A4, Letter, or custom WIDTH,HEIGHT in mm)
l, t, r, b | Margins (in inches, optional)
o | Orientation (Portrait or Landscape)
pagefrom, pageto | Print range


<h4>🧪 Example (REST + Custom Size)</h4>
<pre><code class="language-http">http://localhost:9090/print?url=https://example.com&amp;p=Default&amp;a=77,77&amp;l=0&amp;t=0&amp;r=0&amp;b=0
</code></pre>
<hr>
