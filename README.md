# ESP32 HSPI test

Starts a FreeRTOS task to send HSPI data

See the README.md file in the upper level 'examples' directory for more information about examples.

帧头(1B)	协议版本 (1B)	指令 (1B)	数据(N *bytes)	校验 (1B)	帧尾 (1B)
0xFF	0x01	CMD	DATA	CHECKSUM	0xFE







github upload 
git config --global user.name “fishcan” 
git config --global user.email “fishcan11@sina.cn”
ssh-keygen -C “fishcan11@sina.cn” -t rsa  //windows user .ssh/id_rsa.pub/ copy the text to wab setting/ssh
go to the upload place "git bach here"
git init
git commit -m “CommitInfo
git remote rm origin
git remote add origin git@github.com:fishcan/ESP32-.git
git push -u origin master
