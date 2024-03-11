gdbusxml2cpp使用教程及注意事项

一.客户端使用教程

1.生成客户端使用的接口头文件

在程序目录下打开命令行工具,输入以下命令

./gdbusxml2cpp -i xxx.xml -c

即可生成对应的client_common.h以及interface头文件,客户端包含interface文件即可调用xml定义的接口(在服务端进程已开启的情况下).

2.客户端调用方法和订阅消息

1)方法调用:

实例化一个接口对象,同步方式调用对象中的xxx(方法名)_call_sync函数,异步则调用xxx(方法名)_call_async函数,异步调用时可将等待超时时间和回调函数指针传入参数中.

回调函数示例如下

```c++
static void method_cb(GDBusConnection *connection,
                      GAsyncResult    *res,
                      gpointer         user_data)
{
    GError *error;
    GVariant *result;
    int ret;

    error = NULL;
    result = g_dbus_connection_call_finish (connection,
                                            res,
                                            &error);
    g_variant_get(result,"(i)",&ret);
    g_print("call method set value async ret: %d\n",ret);
}
```

可从result中获取对应方法调用的返回值

2)订阅消息:

从实例化的对象中调用subcribe_signal_xxx(消息名称)的函数即可,消息的回调函数示例如下

```c++
static void signal_cb(GDBusConnection *connection,
                      const gchar *sender_name,const gchar *object_path,
                      const gchar *interface_name,const gchar *signal_name,
                      GVariant *parameters,gpointer user_data)
{
    printf("%s: %s.%s %s\n",object_path,interface_name,signal_name,
           g_variant_print(parameters,FALSE));
}
```

parameters即为消息所带的参数,可使用g_variant_get函数从中解析出各个参数.

二.服务端使用教程

1.生成服务端使用的头文件以及cpp文件

在程序目录下打开命令行工具,输入以下命令

./gdbusxml2cpp -i xxx.xml -s

即可生成对应的server_common.h以及与xml文件同名的服务端头文件和cpp文件,服务端包含server_common.h文件(server_common.h已经包含其他服务端的头文件)即可.

2.开启一个服务

Server_Pointers是一个包含所有xml接口类指针的结构体,用于服务端最终调用,使用时先将结构体指针申请空间,再将实现好的类指针复制给结构体内对应指针,最后将结构体指针作为参数和服务器名称以前传入init函数中,示例如下

```
Server_Pointers *p = (Server_Pointers *)malloc(sizeof (Server_Pointers ));
HzDbusTest *test = new HzDbusTest;
p->test = test;
init("my.test.server.plus",p);
```

对于未实现的类,强烈建议将结构体内的该指针指向NULL(否则可能调用返回错乱值).同步调用未实现的类有可能成功但返回错乱值(指针不是NULL),也可能直接打印服务对象未实现的错误并返回(错误码41).

3.实现对应接口方法

实现cpp文件中的方法,信号无需实现,直接调用类中的emit_signal_xxx(信号名称)即可触发信号.

三.注意事项

1.xml的格式参考标准dbus的xml格式,interface,method,signal的name属性必填,参数的type,name,direction(method的参数direction必填,signal默认为in).现在xml仅支持基本数据类型和数组类型"(av)".

2.cpp中已经默认生成了返回值的类型,特别注意"(av)"类型的构造方法(生成的cpp文件中默认构造了一个仅有一个元素,元素类型为int32,值为0的"(av)"数组).

3.对于同个interface,方法名称和信号名称都不能重名,不同interface的方法信号可以重名,node的名称为路径式/xxx/xxx/...,interface名称为xxx.xxx.xxx最后的后缀用于生成类,所以后缀不要重名,总之xml里的每个name最好都避免重名.

4.编译须添加-L glib-2.0 -L gio-2.0 有可能需要zlib和ffilib







