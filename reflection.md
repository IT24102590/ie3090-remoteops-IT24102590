## 21. Reflection

### 21.1 Which AI tools, if any, did you use, and at which stages of the work?

I used three AI tools during this project. I used Claude mainly while writing and testing code to help debug and clean up the Agent and Controller programs. I used ChatGPT to help polish parts of my report into clear academic English, and Gemini to turn my rough bullet points into concise summaries. The core design, logic, implementation decisions, and testing were my own work. I mainly used AI as a supporting tool during development and refinement.

### 21.2 What did the AI do well, and where did it get things wrong or mislead you?

AI was helpful for understanding how socket calls work and for developing simple helper functions such as `send_all()` and `recv_exact()` to ensure that complete files were transferred correctly. It also helped identify small bugs and improve the clarity of my writing. However, AI sometimes struggled when it did not have the complete context of the assignment brief. For example, some suggested response formats did not always match the protocol table in Section 2.3. For the report, AI sometimes generated generic explanations or described how features should work rather than how my actual implementation worked. Therefore, I had to verify the generated content against my source code, test results, and screenshots.

### 21.3 What did you change, add, or reject from any AI output, and why?

I tested AI suggestions before using them and only kept code that I understood and could verify through testing. I kept the EXEC whitelist strictly hardcoded so that untrusted Controller input could not be passed to the shell. I also ensured that PUT and GET filenames were validated so that files could not escape `./agentfiles/IT24102590/`. I kept `recv_line()` reading one byte at a time so that command processing would not accidentally consume raw file data. I also double-checked all personalized values, including port `9410`, SID `0952`, and authentication token `OPS-2590`, because these values had to match my registration number and assignment requirements.

For the written report, I rewrote sentences that did not accurately describe my implementation and manually corrected figure references to match my screenshots. I also rejected generic AI-generated explanations when they did not match the actual behaviour of my program. This helped ensure that the final report represented my own implementation rather than simply describing an ideal solution.

### 21.4 What did you learn about your own understanding of network programming?

Building this protocol showed me clearly that TCP is a byte stream rather than a message-oriented protocol. A single `recv()` call can return only part of a line or can contain multiple pieces of data, so application-level framing has to be handled explicitly. I also gained a better understanding of why a thread-per-client model works well with blocking I/O, why shared resources such as log files require mutex protection, and how a semaphore can be used to limit concurrent connections to five clients.

Handling unexpected client disconnections also taught me the importance of checking the return values of `send()` and `recv()` and handling `SIGPIPE` appropriately. One difficult area was ensuring that file-transfer operations handled exact byte counts correctly while keeping command processing separate from raw file data. Overall, AI helped speed up my workflow, but I learned that I could only safely rely on AI-generated suggestions after understanding the underlying networking concepts well enough to test and verify them myself.
