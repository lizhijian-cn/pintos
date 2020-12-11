# pintos

# Environment
* ubuntu 20.04
* bochs 2.6.10 

由于使用的Ubuntu20.04移除了一些32位库和某些不知情的原因，一直无法成功编译安装bochs和pintos。感谢这位同学提供的移植到20.04的pintos，我也做了一些摸索，删除了pintos中的第564行
```
user_shortcut: keys=ctrlaltdel
```
现在已经可以编译运行了，但**无法直接在bochs下调试**，必须安装`qemu-system`用qemu进行调试。


# Install
```sh
git clone https://github.com/lizhijian-cn/pintos.git

cd pintos

echo "export PINTOS_HOME=$PWD" >> ~/.zshrc

echo "export PATH=\$PATH:\$PINTOS_HOME/src/utils" >> ~/.zshrc

source ~/.zshrc

wget https://sourceforge.net/projects/bochs/files/bochs/2.6.10/bochs-2.6.10.tar.gz

tar xzf bochs-2.6.10.tar.gz

cd bochs-2.6.10

```
在`.conf.linux`的line 73添加（虽然因为无法调试，打开gdb并没啥用）
```
--enable-gdb-stub \
--with-nogui \
```
然后编译安装
```
./.conf.linux
make
sudo make install
```
一切顺利的话，来到`$PINTOS_HOME/src/threads`后`make check`应该可以通过7个点
