# HolyLogs
This is a logging server used for HolyLib since Loki is really bad for my usecase and I couldn't find any other good logging servers.<br>

We store data for a very specific case.<br>
We have Log Indexes that have **one** indentifier and entries that it stores which later on can be queued<br>
We heavily use the filesystem and try to keep a low memory usage for all cases<br>

> [!NOTE]
> Currently, every index can be modified by only **one** thread