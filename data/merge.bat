
:: video_title is too big for github
@echo on

copy /b .\xa* video_title.txt.tgz
:: copy /b xaa+xab+xac+xad video_title.txt.tgz

:: type command is too slower than copy /b, because it like cat command on Linux.

:: for /R . %%f in (xa*) do type %%f >> video_title.txt.tgz
:: type .\xa* >> video_title.txt.tgz
