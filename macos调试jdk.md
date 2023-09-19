# macos+clion调试jdk源代码

## 1、升级homebrew

- `cd "$(brew --repo)"`
- `git remote set-url origin https://mirrors.ustc.edu.cn/brew.git`设置homebrew的仓库
- `echo 'export HOMEBREW_BOTTLE_DOMAIN=https://mirrors.ustc.edu.cn/homebrew-bottles' >> ~/.zshrc`

## 2、打开openjdk的官网

![image-20230912091304775](https://p.ipic.vip/ljj7ij.png)

![image-20230912091414031](https://p.ipic.vip/kt4grn.png)

可以看到macos需要预先安装`xcode`相对来说编译起来更加复杂一些。

![image-20230912091655599](https://p.ipic.vip/8hdyxg.png)

事实证明确实有很大的问题。

### 查看macos需要安装哪些工具

- `xcode-select --install`当然Xcode必须要有
- `brew install freetype`
- `brew install autoconf`
- `brew install ccache`

# 运行configure配置编译环境

1. cd到源代码目录
2. `bash configure --disable-warnings-as-errors --with-debug-level=slowdebug --with-jvm-variants=server`
   1. `disable-warnings-as-errors`选项是禁止把warning 当成error
   2. `--with-debug-level=slowdebug`。用来设置编译的级别，可选值为release、fastdebug、slowde-bug，越往后进行的优化措施就越少，带的调试信息就越多。默认值为release。slowdebug 含有最丰富的调试信息，没有这些信息，很多执行可能被优化掉，我们单步执行时，可能看不到一些变量的值。所以最好指定slowdebug 为编译级别。
   3. `with-jvm-variants` 编译特定模式的HotSpot虚拟机，可选值：server、client、minimal、core、zero、custom
   4. `configure` 命令承担了依赖项检查、参数配置和构建输出目录结构等多项职责，如果编译过程中需要的工具链或者依赖项有缺失，命令执行后会得到明确的提示，并给出该依赖的安装命令。
3. `make images`开始build

## 问题如何解决

### 1 运行`bash configure --disable-warnings-as-errors --with-debug-level=slowdebug --with-jvm-variants=server`遇到Xcode安装问题

![image-20230912102715315](https://p.ipic.vip/jkajdi.png)

如果是刚刚新安装的`xcode`，可能会遇到这个问题，这么解决：

1. 打开Xcode，完成安装，安装命令行参数：`xcode-select --install`
2. `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer`设置命令行参数工具的路径
3. 然后可以继续运行`bash configure --disable-warnings-as-errors --with-debug-level=slowdebug --with-jvm-variants=server`试试

### 2 运行`bash configure --disable-warnings-as-errors --with-debug-level=slowdebug --with-jvm-variants=server`遇到boot jdk设置问题

![image-20230912103145001](https://p.ipic.vip/reythn.png)

问题本质是编译版本号为N的JDK，必须要安装N-1的JDK才行。

所以，去oracle官网下载macos的对应版本就能解决。[这里](https://www.oracle.com/java/technologies/downloads/archive/)

如果系统中有多个JDK版本，要切换的话，可以这样：

- `/usr/libexec/java_home -V` 查看系统中存在的JDK

![image-20230912104659066](https://p.ipic.vip/zaln6n.png)

- ```bash
  usr/libexec/java_home -v 11.0.12 #切换版本
  ```

- 在~/.zshrc写入

```bash
export JAVA_HOME=$(/usr/libexec/java_home -v 11.0.12)
export PATH=$JAVA_HOME/bin:$PATH
export CLASS_PATH=$JAVA_HOME/lib
```
- `source ~/.zshrc`即可

### 3 `make images`的问题——关于不同芯片的问题

- macOS（M1系列芯片） 需要jdk>=17才能编译，主要是`boot jdk`不识别M1芯片，17以后才支持。当然可以试试别的vendor的JDK，可能有针对M系列的build。

- macOS——intel芯片，这个问题主要是

  - assert()编译问题，需要调换`unittest.hpp`头文件引用的顺序。[参考](https://stackoverflow.com/questions/72336864/how-to-compile-openjdk-11-on-macos)。

  - 由Xcode-14带来的问题：`**error in Xcode File not found: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/arc/lib***.a**`找不到链接目标文件。[参考](https://developer.apple.com/forums/thread/728021)。

    步骤是：

    1、sudo mkdir arc

    2、cd arc

    3、sudo git clone [git@github.com](mailto:git@github.com):kamyarelyasi/Libarclite-Files.git

### 4 `make images`成功后

在`jdk-jdk-20-ga/build/macosx-aarch64-server-slowdebug/jdk/bin`下能找到`java`的执行文件。

### 5 导入clion

1. cd到源代码目录，运行`make compile-commands`
2. 会在`build/macosx-aarch64-server-slowdebug`产生`compile_commands.json`文件
3. 在Clion里面打开

![image-20230912135804660](/Users/xiaojin/workshop/md_files/md_images/image-20230912135804660.png)

选择`Open as Project`

![image-20230912140119811](https://p.ipic.vip/snpglk.png)		

4. 可以看到，并没有源代码，这时我们修改源代码路径

![image-20230912141040897](https://p.ipic.vip/rfvp25.png)

选择源代码根目录。完成加载。

5. 接着开始设置`build configuration`，选择`custom build application`

![image-20230912141404010](/Users/xiaojin/workshop/md_files/md_images/image-20230912141404010.png)

点击`Configure Custom Build Targets`

![image-20230912141508908](https://p.ipic.vip/hi3fol.png)

![image-20230912141539863](https://p.ipic.vip/40i0v2.png)

![image-20230912141608509](https://p.ipic.vip/70kceb.png)

然后设置build与clean

![image-20230912141721857](https://p.ipic.vip/6oxl4m.png)

build——`make images CONF=macosx-aarch64-server-slowdebug`

clean——`make images CONF=macosx-aarch64-server-slowdebug clean`



![image-20230912141943747](https://p.ipic.vip/plonks.png)

![image-20230912142038321](https://p.ipic.vip/l1xx3v.png)

完成设置：

![image-20230912142115004](https://p.ipic.vip/2ei96x.png)

然后在`configuration build`选择`make`这个target配置，并设置`Executable`的位置，也就是`编译好的java`二进制的位置，并设置`arguments`为`--version`

![image-20230912142407330](https://p.ipic.vip/bb54uf.png)

点击`run`就可以看到结果了

![image-20230912142514532](https://p.ipic.vip/zjt6wo.png)

同样可以可以调试了

![image-20230912142947722](https://p.ipic.vip/erq651.png)
