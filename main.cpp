#include <iostream>
#include "tinyxml2.h"
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <getopt.h>

#define MAX_INTERFACE_NUM 500

using namespace std;
using namespace tinyxml2;

static int interface_count(const char* xml)
{
    int count = 0;
    XMLDocument xmldoc;
    xmldoc.LoadFile(xml);
    XMLElement* root = xmldoc.RootElement();
    if(root == NULL)
    {

        printf("xml parse failed!\n");
        return -1;
    }

    XMLNode* pInterface  = NULL;
    for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
    {
        if(pInterface->ToElement()->Attribute("name") != NULL)
            count++;
    }

    return count;
}
static void parse_include_xml(const char* mainxml,vector<string> &token)
{
    XMLDocument xmldoc;
    xmldoc.LoadFile(mainxml);
    XMLElement* root = xmldoc.RootElement();
    XMLNode* file  = NULL;
    if(root != NULL)
    {
        const char* objPath = root->Attribute("xmlns:xi");
        if(objPath == NULL)
        {
            cout << "main xml file parse error , try parse xml directly" << endl;
            token.push_back(string(mainxml));
            return;
        }
        cout << "objpath : " << objPath << endl;
        for(file=root->FirstChildElement();file;file=file->NextSiblingElement())
        {
            if(file->ToElement()->Attribute("href") != NULL)
            {
                cout << file->ToElement()->Attribute("href") << endl;
                token.push_back(string(file->ToElement()->Attribute("href")));
            }else{
                cout << "no xml file included!" << endl;
                return;
            }
        }
    }else{
        printf("xml parse failed!\n");
        return ;
    }
}

static string create_class_name(const char* interface)
{
    string inter(interface);
    size_t found = inter.find_last_of('.');
    string prefix = inter.substr(found+1);
    prefix[0] = toupper(prefix[0]);
    prefix += "Interface";
    cout << "class name is " << prefix << endl;
    return prefix;
}

static string create_sever_class_name(const char* interface)
{
    string inter(interface);
    size_t found = inter.find_last_of('.');
    string prefix = inter.substr(found+1);
    prefix[0] = toupper(prefix[0]);
    prefix = "HzDbus"  + prefix;
    cout << "server class name is " << prefix << endl;
    return prefix;
}

static string get_interface_suffix(const char* interface)
{
    string inter(interface);
    size_t found = inter.find_last_of('.');
    string suffix = inter.substr(found+1);
    return suffix;
}

static string getRealType(string type)
{
    if((type.compare("i") == 0) || (type.compare("(i)") == 0))
    {
        return string("int ");//INT32
    }else if((type.compare("s") == 0) || (type.compare("(s)") == 0))
    {
        return string("const char *");//
    }else if((type.compare("b") == 0) || (type.compare("(b)") == 0))
    {
        return string("bool ");
    }else if((type.compare("n") == 0) || (type.compare("(n)") == 0))
    {
        return string("short ");//INT16
    }else if((type.compare("u") == 0) || (type.compare("(u)") == 0))
    {
        return string("uint ");//UINT32
    }else if((type.compare("q") == 0) || (type.compare("(q)") == 0))
    {
        return string("ushort ");//UINT16
    }else if((type.compare("x") == 0) || (type.compare("(x)") == 0))
    {
        return string("long long ");//INT64
    }else if((type.compare("t") == 0) || (type.compare("(t)") == 0))
    {
        return string("unsigned long long ");//UINT64
    }else if((type.compare("d") == 0) || (type.compare("(d)") == 0))
    {
        return string("double ");//double
    }else if((type.compare("y") == 0) || (type.compare("(y)") == 0))
    {
        return string("uchar ");//BTYE
    }else if((type.compare("av") == 0) || (type.compare("(av)") == 0))
    {
        return string("GVariant* ");//VARIANT
    }
    return string("");
}

static string generateErrReturnValue(string type)
{
    if((type.compare("i") == 0) || (type.compare("(i)") == 0))
    {
        return string(" return -100;\n");//INT32
    }else if((type.compare("s") == 0) || (type.compare("(s)") == 0))
    {
        return string(" return NULL;\n");//
    }else if((type.compare("b") == 0) || (type.compare("(b)") == 0))
    {
        return string(" return false;\n");
    }else if((type.compare("n") == 0) || (type.compare("(n)") == 0))
    {
        return string(" return -100;\n");//INT16
    }else if((type.compare("u") == 0) || (type.compare("(u)") == 0))
    {
        return string(" return UINT32_MAX;\n");//UINT32
    }else if((type.compare("q") == 0) || (type.compare("(q)") == 0))
    {
        return string(" return UINT16_MAX;\n");//UINT16
    }else if((type.compare("x") == 0) || (type.compare("(x)") == 0))
    {
        return string(" return -100;\n");//INT64
    }else if((type.compare("t") == 0) || (type.compare("(t)") == 0))
    {
        return string(" return UINT64_MAX;\n");//UINT64
    }else if((type.compare("d") == 0) || (type.compare("(d)") == 0))
    {
        return string(" return -1;\n");//double
    }else if((type.compare("y") == 0) || (type.compare("(y)") == 0))
    {
        return string(" return 255;\n");//BYTE
    }else if((type.compare("av") == 0) || (type.compare("(av)") == 0))
    {
        return string(" return NULL;\n");//VARIANT
    }
    return string(" return;\n");
}

static int generate_interface(const char* xmlfile,string filename)
{
    if(xmlfile == NULL || filename.empty())
    {
        printf("file path error!\n");
        return -2;
    }

    string head_file_content = "";
    string head_fun_sync = filename;
    transform(head_fun_sync.begin(),head_fun_sync.end(),head_fun_sync.begin(),::toupper);

    head_fun_sync += "_H";

    head_file_content += "/*********************************************\n";
    head_file_content += "The GDBus XML compiler is a tool that can be used to parse interface descriptions and produce\n";
    head_file_content += "static code representing those interfaces, which can then be used to make calls to remote\n";
    head_file_content += "objects or implement said interfaces.\n";
    head_file_content += "*********************************************/\n";

    head_file_content += "#ifndef ";
    head_file_content += head_fun_sync;
    head_file_content += "\n";
    head_file_content += "#define ";
    head_file_content += head_fun_sync;
    head_file_content += "\n";
    head_file_content += "\n";
    head_file_content += "#include <iostream>\n";
    head_file_content += "#include <stdio.h>\n";
    head_file_content += "#include <stdlib.h>\n";
    head_file_content += "#include <gio/gio.h>\n";
    head_file_content += "#include \"client_common.h\"\n";

    head_file_content += "\n";
    head_file_content += "using namespace std;\n";

    head_file_content += "\n";
    head_file_content += "extern GDBusConnection *c;\n";
    head_file_content += "extern const char* server_name;\n";
    head_file_content += "extern bool initFlag;\n";

//    head_file_content += "\n";
//    head_file_content += "int initDbus(const char* name)\n";
//    head_file_content += "{\n";
//    head_file_content += "  server_name = name;\n";
//    head_file_content += "  c = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);\n";
//    head_file_content += "  if(c == NULL)\n";
//    head_file_content += "      return -1;\n";
//    head_file_content += "  return 0;\n";
//    head_file_content += "}\n";
//    head_file_content += "\n";
    

    filename += ".h";

    const char* objPath = NULL;
    vector<string> interfaces;
    XMLDocument xmldoc;
    xmldoc.LoadFile(xmlfile);
    XMLElement* root = xmldoc.RootElement();
    if(root != NULL)
    {
        objPath = root->Attribute("name");
        cout << "objpath : " << objPath << endl;
    }else{
        printf("xml parse failed!\n");
        return -1;
    }

    XMLNode* pInterface  = NULL;
    XMLNode *pMethod_Signal = NULL;
    XMLNode *pArgs = NULL;

    for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
    {

        if(pInterface->ToElement()->Attribute("name") == NULL)
        {
            cout << "ERR: Interface without a name,please check it!" << endl;
            return -3;
        }
        //interfaces.push_back(string(pInterface->ToElement()->Attribute("name")));
        head_file_content += "//";
        head_file_content += pInterface->ToElement()->Attribute("name");
        head_file_content += "\n";

        head_file_content += "class ";
        string class_name = create_class_name(pInterface->ToElement()->Attribute("name"));
        head_file_content += class_name;
        head_file_content += " {\n";
        head_file_content += "public: \n";
        head_file_content += "    ";
        head_file_content += class_name;
        head_file_content += "(){}\n";
        head_file_content += "    ";
        head_file_content += "virtual ~";
        head_file_content += class_name;
        head_file_content += "(){}\n";
        head_file_content += "\n";


        for(pMethod_Signal = pInterface->FirstChildElement(); pMethod_Signal; pMethod_Signal = pMethod_Signal->NextSiblingElement())
        {
            if(pMethod_Signal->ToElement()->Value() == NULL)
            {
                cout << "ERR: method or signal without type,please check it!" << endl;
                return -3;
            }

            if(pMethod_Signal->ToElement()->Attribute("name") == NULL)
            {
                cout << "ERR: method or signal without a name,please check it!" << endl;
                return -3;
            }

            string type = pMethod_Signal->ToElement()->Value();
            string method_signal_name = pMethod_Signal->ToElement()->Attribute("name");
            string arg_name,arg_type,dir,out_type,arg_list,arg_list_origin,arg_types_in;



            if(type.compare("method") == 0)//handle method
            {

                for(pArgs = pMethod_Signal->FirstChildElement(); pArgs; pArgs = pArgs->NextSiblingElement())
                {

                    if(pArgs->ToElement()->Attribute("name") != NULL)
                        arg_name = pArgs->ToElement()->Attribute("name");
                    else{
                        cout << "ERR: Args without name,please check it!" << endl;
                        return -3;
                    }

                    if(pArgs->ToElement()->Attribute("type") != NULL)
                        arg_type = pArgs->ToElement()->Attribute("type");
                    else{
                        cout << "ERR: Args without type,please check it!" << endl;
                        return -3;
                    }

                    if(pArgs->ToElement()->Attribute("direction") != NULL)
                    {
                        dir = pArgs->ToElement()->Attribute("direction");
                        if((dir.compare("out") == 0))
                        {
                            out_type = getRealType(arg_type);

                        }else{

                            arg_list += getRealType(arg_type);
                            arg_list += arg_name;

                            arg_list += ",";


                            arg_list_origin += arg_name;
                            arg_list_origin += ",";

                            arg_types_in += pArgs->ToElement()->Attribute("type");
                        }

                    }else{
                        arg_list += getRealType(arg_type);
                        arg_list += arg_name;

                        arg_list += ",";


                        arg_list_origin += arg_name;
                        arg_list_origin += ",";

                        arg_types_in += pArgs->ToElement()->Attribute("type");
                    }

                }

                if(arg_list.size() > 0)
                    arg_list.pop_back();
                else
                    arg_list = string("");

                string fun_sync = "";
                string fun_async = "";

                //sync functions
                fun_sync += "    ";
                fun_sync += out_type;
                //                fun_sync += create_class_prefix(pInterface->ToElement()->Attribute("name"));
                //                fun_sync += "_";
                fun_sync += method_signal_name;
                fun_sync += "_call_sync(";
                fun_sync += arg_list;

                fun_sync += ")\n";
                fun_sync += "    {\n";
                fun_sync += "       if(c == NULL)\n";
                fun_sync += "       {\n";
                fun_sync += "           cout << \"GDbus Connection not inited !\" << endl;\n";
                fun_sync += "          ";
                //fun_sync += "       return NULL;\n";
                fun_sync += generateErrReturnValue(arg_type);

                fun_sync += "       }\n";
                fun_sync += "       GError *err = NULL;\n";
                fun_sync += "       GVariant* response =  g_dbus_connection_call_sync(c,\n";
                fun_sync += "                                                         server_name,\n";
                fun_sync += "                                                         \"";
                fun_sync += root->Attribute("name");
                fun_sync +="\",\n";
                fun_sync += "                                                         \"";
                fun_sync += pInterface->ToElement()->Attribute("name");
                fun_sync +="\",\n";
                fun_sync += "                                                         \"";
                fun_sync += method_signal_name;
                fun_sync +="\",\n";
                fun_sync += "                                                         ";
                if(arg_list_origin.size() > 0)
                    arg_list_origin.pop_back();
                else
                    arg_list_origin = string("");

                if(arg_list_origin.size() > 0)
                    fun_sync += "g_variant_new (\"("+ arg_types_in +")\"," + arg_list_origin + "),\n";
                else
                    fun_sync += "NULL,\n";

                fun_sync += "                                                         ";
                fun_sync += "G_VARIANT_TYPE(";
                fun_sync += "\"(" + arg_type + ")\"),\n";
                fun_sync += "                                                         ";
                fun_sync += "G_DBUS_CALL_FLAGS_NONE,\n";
                fun_sync += "                                                         ";
                fun_sync += "-1,\n";
                fun_sync += "                                                         ";
                fun_sync += "NULL,\n";
                fun_sync += "                                                         ";
                fun_sync += "&err);\n";

                if(out_type.compare("GVariant* ") == 0)
                {
                    fun_sync += "      if(response != NULL)\n";
                    fun_sync += "      {\n";
                    fun_sync += "            return response;\n";
                    fun_sync += "      }else{\n";
                    fun_sync += "            g_print(\"err: %s \\n\",err->message);\n";
                    fun_sync += "            return NULL;\n";
                    fun_sync += "      }\n";
                }else{
                    fun_sync += "       "+ out_type + "ret;\n";
                    fun_sync += "      if(response != NULL)\n";
                    fun_sync += "      {\n";
                    fun_sync += "            g_variant_get(response,\"("+ arg_type +")\",&ret);\n";
                    fun_sync += "      }else{\n";
                    fun_sync += "            g_print(\"err: %s \\n\",err->message);\n";
                    fun_sync += "      }\n";
                    fun_sync += "       return ret;\n";
                }

                fun_sync += "    }\n";
                fun_sync += "\n";

                //async  functions
                fun_async += "    void ";
                //                fun_async += creat_c_prefix(pInterface->ToElement()->Attribute("name"));
                //                fun_async += "_";
                fun_async += method_signal_name;
                fun_async += "_call_async(";
                fun_async += arg_list;
                if(arg_list.size() == 0)
                    fun_async += "int timeout_msec = -1,GAsyncReadyCallback cb = NULL)\n";
                else
                    fun_async += ",int timeout_msec = -1,GAsyncReadyCallback cb = NULL)\n";
                fun_async += "    {\n";
                fun_async += "       if(c == NULL)\n";
                fun_async += "       {\n";
                fun_async += "           cout << \"GDbus Connection not inited !\" << endl;\n";
                fun_async += "          ";

                fun_async += generateErrReturnValue(string("void"));
                fun_async += "       }\n";
                fun_async += "       g_dbus_connection_call(c,\n";
                fun_async += "                              server_name,\n";
                fun_async += "                              \"";
                fun_async += root->Attribute("name");
                fun_async +="\",\n";
                fun_async += "                              \"";
                fun_async += pInterface->ToElement()->Attribute("name");
                fun_async +="\",\n";
                fun_async += "                              \"";
                fun_async += method_signal_name;
                fun_async +="\",\n";
                fun_async += "                              ";
                if(arg_list_origin.size() > 0)
                    fun_async += "g_variant_new (\"("+ arg_types_in +")\"," + arg_list_origin + "),\n";
                else
                    fun_async += "NULL,\n";
                fun_async += "                              ";
                fun_async += "NULL,\n";
                //                fun_async += "G_VARIANT_TYPE(";
                //                fun_async += "\"(" + arg_type + ")\"),\n";
                fun_async += "                              ";
                fun_async += "G_DBUS_CALL_FLAGS_NONE,\n";
                fun_async += "                              ";
                fun_async += "timeout_msec,\n";
                fun_async += "                              ";
                fun_async += "NULL,\n";
                fun_async += "                              ";
                fun_async += "cb,\n";
                fun_async += "                              ";
                fun_async += "NULL);\n";

                fun_async += "    }\n";
                fun_async += "\n";

                head_file_content += fun_sync;
                head_file_content += fun_async;

            }else if(type.compare("signal") == 0)//handle signal
            {

                string fun_signal = "";

                fun_signal += "    int subcribe_signal_";
                fun_signal += method_signal_name;
                fun_signal += "(GDBusSignalCallback cb)\n";
                fun_signal += "    {\n";
                fun_signal += "       if(c == NULL)\n";
                fun_signal += "       {\n";
                fun_signal += "           cout << \"GDbus Connection not inited !\" << endl;\n";
                fun_signal += "           return -1;\n";
                fun_signal += "       }\n";
                fun_signal += "       return g_dbus_connection_signal_subscribe(c,server_name,\"";
                fun_signal += pInterface->ToElement()->Attribute("name");
                fun_signal += "\",\"";
                fun_signal += method_signal_name;
                fun_signal += "\",\"";
                fun_signal += objPath;
                fun_signal += "\",NULL,\n";
                fun_signal += "                              G_DBUS_SIGNAL_FLAGS_NONE,cb,NULL,NULL);\n";
                fun_signal += "    }\n";
                fun_signal += "\n";

                head_file_content += fun_signal;

            }
        }

        head_file_content += "};\n";
    }

    head_file_content +="#endif";

    ofstream outfile;
    outfile.open(filename, ios_base::out | ios_base::trunc);  //删除文件重写
    stringstream ss;
    ss << head_file_content;
    outfile << ss.str() << endl;
    outfile.close();
    cout << "generate interface done !" << endl;
    return 0;
}

static int generate_client_common_file(vector<string>& filenames)
{
    string out("client_common.h");
    string head_fun_sync("client_common");
    transform(head_fun_sync.begin(),head_fun_sync.end(),head_fun_sync.begin(),::toupper);

    head_fun_sync += "_H";



    string content = "";

    content += "#ifndef ";
    content += head_fun_sync;
    content += "\n";
    content += "#define ";
    content += head_fun_sync;
    content += "\n";
    content += "#include <gio/gio.h>\n";

    content += "\n";
    content += "using namespace std;\n";

    content += "\n";
    content += "GDBusConnection *c = NULL;\n";
    content += "const char* server_name = NULL;\n";
    content += "bool initFlag = false;\n";

    content += "\n";
    content += "int initDbus(const char* name)\n";
    content += "{\n";
    content += "  server_name = name;\n";
    content += "  c = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);\n";
    content += "  if(c == NULL)\n";
    content += "      return -1;\n";
    content += "  return 0;\n";
    content += "}\n";
    content += "\n";

    content +="#endif";

    ofstream outfile;
    outfile.open(out, ios_base::out | ios_base::trunc);  //删除文件重写
    stringstream ss;
    ss << content;
    outfile << ss.str() << endl;
    outfile.close();
    cout << "generate client common done !" << endl;
    return 0;
}
static int generate_server_cpp_file(const char* xmlfile,string filename)
{
    transform(filename.begin(),filename.end(),filename.begin(),::tolower);
    string classheaderfile = filename + ".cpp";
    const char* outfile = classheaderfile.c_str();

    string cpp_file_contents = "";
    cpp_file_contents = "#include \"" + filename + ".h\"\n";

    if (!filename.empty())
        filename[0] = toupper(filename[0]);
    string classname = filename;

    cpp_file_contents += "\n";


    const char* objPath = NULL;
    vector<string> interfaces;
    XMLDocument xmldoc;
    xmldoc.LoadFile(xmlfile);
    XMLElement* root = xmldoc.RootElement();
    if(root != NULL)
    {
        objPath = root->Attribute("name");
        cout << "objpath : " << objPath << endl;
    }else{
        printf("xml parse failed!\n");
        return -1;
    }

    XMLNode* pInterface  = NULL;
    XMLNode *pMethod_Signal = NULL;
    XMLNode *pArgs = NULL;

    for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
    {
        string interface(pInterface->ToElement()->Attribute("name"));
        string serverclass = create_sever_class_name(interface.c_str());

        cpp_file_contents += serverclass + "::" + serverclass + "()\n";
        cpp_file_contents += "{\n";
        cpp_file_contents += "\n";
        cpp_file_contents += "}\n";
        cpp_file_contents += "\n";

        cpp_file_contents += serverclass + "::~" + serverclass + "()\n";
        cpp_file_contents += "{\n";
        cpp_file_contents += "\n";
        cpp_file_contents += "}\n";
        cpp_file_contents += "\n";

        cpp_file_contents += "void " + serverclass + "::setConnection(GDBusConnection *c)\n";
        cpp_file_contents += "{\n";
        cpp_file_contents += "    if(c == NULL)\n";
        cpp_file_contents += "    {\n";
        cpp_file_contents += "        g_print(\"connection failed\\n\");\n";
        cpp_file_contents += "        return;\n";
        cpp_file_contents += "    }\n";
        cpp_file_contents += "    conn = c;\n";
        cpp_file_contents += "}\n";
        cpp_file_contents += "\n";

        for(pMethod_Signal = pInterface->FirstChildElement(); pMethod_Signal; pMethod_Signal = pMethod_Signal->NextSiblingElement())
        {

            string type = pMethod_Signal->ToElement()->Value();
            string method_signal_name = pMethod_Signal->ToElement()->Attribute("name");
            string arg_name,arg_type,dir,out_type,arg_list,arg_type_in,arg_list_origin;

            if(type.compare("method") == 0)//handle method
            {
                for(pArgs = pMethod_Signal->FirstChildElement(); pArgs; pArgs = pArgs->NextSiblingElement())
                {

                    if(pArgs->ToElement()->Attribute("name") != NULL)
                        arg_name = pArgs->ToElement()->Attribute("name");

                    if(pArgs->ToElement()->Attribute("type") != NULL)
                        arg_type = pArgs->ToElement()->Attribute("type");

                    if(pArgs->ToElement()->Attribute("direction") != NULL)
                    {
                        dir = string(pArgs->ToElement()->Attribute("direction"));
                    }else{
                        dir = string("");
                    }
                    if((dir.compare("out") == 0))
                    {
                        out_type = getRealType(arg_type);

                    }else{

                        arg_list += getRealType(arg_type);
                        arg_list += arg_name;
                        arg_list += ",";

                    }

                    //                    }

                }

                if(arg_list.size() > 0)
                    arg_list.pop_back();
                else
                    arg_list = string("");

                cpp_file_contents += out_type + serverclass + "::" + method_signal_name + "(" + arg_list + ")\n";
                cpp_file_contents += "{\n";
                if(out_type.compare("GVariant* ") == 0)
                {
                   cpp_file_contents += "    " + out_type + "ret = NULL;\n";
                   cpp_file_contents += "    GVariantBuilder b;\n";
                   cpp_file_contents += "    g_variant_builder_init(&b,G_VARIANT_TYPE_ARRAY);\n";
                   cpp_file_contents += "    g_variant_builder_add_value(&b,g_variant_new(\"v\",g_variant_new(\"(i)\",0)));\n";
                   cpp_file_contents += "    ret = g_variant_new(\"(av)\",&b);\n";
                }else{
                    cpp_file_contents += "    " + out_type + "ret;\n";
                }
                cpp_file_contents += "    return ret;\n";
                cpp_file_contents += "}\n";
                cpp_file_contents += "\n";

            }else if(type.compare("signal") == 0)
            {
                for(pArgs = pMethod_Signal->FirstChildElement(); pArgs; pArgs = pArgs->NextSiblingElement())
                {
                    if(pArgs->ToElement()->Attribute("name") != NULL)
                        arg_name = pArgs->ToElement()->Attribute("name");

                    if(pArgs->ToElement()->Attribute("type") != NULL)
                        arg_type = pArgs->ToElement()->Attribute("type");

                    arg_list += getRealType(arg_type);
                    arg_list += arg_name;
                    arg_list += ",";

                    arg_type_in += arg_type;

                    arg_list_origin += arg_name + ",";
                }

                if(arg_list.size() > 0)
                    arg_list.pop_back();
                else
                    arg_list = string("");

                if(arg_list_origin.size() > 0)
                    arg_list_origin.pop_back();
                else
                    arg_list_origin = string("");

                cpp_file_contents += "void " + serverclass + "::emit_" + method_signal_name + "(" + arg_list + ")\n";
                cpp_file_contents += "{\n";
                cpp_file_contents += "    if(conn == NULL)\n";
                cpp_file_contents += "    {\n";
                cpp_file_contents += "        g_print(\"connection failed\\n\");\n";
                cpp_file_contents += "        return;\n";
                cpp_file_contents += "    }\n";
                cpp_file_contents += "    if(g_dbus_connection_emit_signal(conn,NULL,\"";
                cpp_file_contents += string(objPath) +"\",\"" + interface +"\",\"" + method_signal_name + "\",";
                cpp_file_contents += "g_variant_new (\"(" + arg_type_in +")\"," + arg_list_origin + "),NULL))\n";
                cpp_file_contents += "    {\n";
                cpp_file_contents += "        cout << \"emit a signal successfully!\" << endl;\n";
                cpp_file_contents += "    }else{\n";
                cpp_file_contents += "        cout << \"emit a signal failed!\" << endl;\n";
                cpp_file_contents += "    }\n";
                cpp_file_contents += "}\n";
                cpp_file_contents += "\n";


            }
        }
    }

    cout << cpp_file_contents << endl;
    ofstream cpp_file;
    cpp_file.open(outfile, ios_base::out | ios_base::trunc);  //删除文件重写
    stringstream ss_cpp;
    ss_cpp << cpp_file_contents;
    cpp_file << ss_cpp.str() << endl;
    cpp_file.close();
    cout << "generate server cpp file done !" << endl;

    return 0;
}

static int generate_server_head_file(const char* xmlfile,string filename)
{
    transform(filename.begin(),filename.end(),filename.begin(),::tolower);
    string classheaderfile = filename + ".h";
    const char* outfile = classheaderfile.c_str();

    if (!filename.empty())
        filename[0] = toupper(filename[0]);
    string classname = filename;

    string head_file_contents = "";
    string head_def = filename;
    transform(head_def.begin(),head_def.end(),head_def.begin(),::toupper);
    head_def += "_H";
    head_file_contents += "#ifndef ";
    head_file_contents += head_def;
    head_file_contents += "\n";
    head_file_contents += "#define ";
    head_file_contents += head_def;
    head_file_contents += "\n";
    head_file_contents += "#include <iostream>\n";
    head_file_contents += "#include <stdlib.h>\n";
    head_file_contents += "#include <fstream>\n";
    head_file_contents += "#include <cstdlib>\n";
    head_file_contents += "#include <unistd.h>\n";
    head_file_contents += "#include <string.h>\n";
    head_file_contents += "#include <glib-2.0/glib.h>\n";
    head_file_contents += "#include <gio/gio.h>\n";
    head_file_contents += "\n";
    head_file_contents += "using namespace std;\n";
    head_file_contents += "\n";

    const char* objPath = NULL;
    vector<string> interfaces;
    XMLDocument xmldoc;
    xmldoc.LoadFile(xmlfile);
    XMLElement* root = xmldoc.RootElement();
    if(root != NULL)
    {
        objPath = root->Attribute("name");
        cout << "objpath : " << objPath << endl;
    }else{
        printf("xml parse failed!\n");
        return -1;
    }

    XMLNode* pInterface  = NULL;
    XMLNode *pMethod_Signal = NULL;
    XMLNode *pArgs = NULL;

    for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
    {

        //interfaces.push_back(string(pInterface->ToElement()->Attribute("name")));
        head_file_contents += "//";
        head_file_contents += pInterface->ToElement()->Attribute("name");
        head_file_contents += "\n";

        string serverclass = create_sever_class_name(pInterface->ToElement()->Attribute("name"));

        head_file_contents += "class " + serverclass + "\n";
        head_file_contents += "{\n";
        head_file_contents += "public:\n";
        head_file_contents += "    " + serverclass +"();\n";
        head_file_contents += "    virtual ~" + serverclass + "();\n";
        head_file_contents += "\n";
        head_file_contents += "    void setConnection(GDBusConnection *c);\n";


        for(pMethod_Signal = pInterface->FirstChildElement(); pMethod_Signal; pMethod_Signal = pMethod_Signal->NextSiblingElement())
        {

            string type = pMethod_Signal->ToElement()->Value();
            string method_signal_name = pMethod_Signal->ToElement()->Attribute("name");
            string arg_name,arg_type,dir,out_type,arg_list;

            if(type.compare("method") == 0)//handle method
            {

                for(pArgs = pMethod_Signal->FirstChildElement(); pArgs; pArgs = pArgs->NextSiblingElement())
                {

                    if(pArgs->ToElement()->Attribute("name") != NULL)
                        arg_name = pArgs->ToElement()->Attribute("name");

                    if(pArgs->ToElement()->Attribute("type") != NULL)
                        arg_type = pArgs->ToElement()->Attribute("type");

                    if(pArgs->ToElement()->Attribute("direction") != NULL)
                    {
                        dir = string(pArgs->ToElement()->Attribute("direction"));
                    }else{
                        dir = string("");
                    }
                    if((dir.compare("out") == 0))
                    {
                        out_type = getRealType(arg_type);

                    }else{

                        arg_list += getRealType(arg_type);
                        arg_list += arg_name;
                        arg_list += ",";

                    }

                    //                    }

                }

                if(arg_list.size() > 0)
                    arg_list.pop_back();
                else
                    arg_list = string("");

                head_file_contents += "    " + out_type + method_signal_name +"(" + arg_list + ");\n";

            }else if(type.compare("signal") == 0)//handle signal
            {
                //cout << "signal name: " << method_signal_name << endl;
                for(pArgs = pMethod_Signal->FirstChildElement(); pArgs; pArgs = pArgs->NextSiblingElement())
                {
                    if(pArgs->ToElement()->Attribute("name") != NULL)
                        arg_name = pArgs->ToElement()->Attribute("name");

                    if(pArgs->ToElement()->Attribute("type") != NULL)
                        arg_type = pArgs->ToElement()->Attribute("type");

                    arg_list += getRealType(arg_type);
                    arg_list += arg_name;
                    arg_list += ",";
                }

                if(arg_list.size() > 0)
                    arg_list.pop_back();
                else
                    arg_list = string("");


                head_file_contents += "    void emit_" + method_signal_name + "(" + arg_list + ");\n";

            }
        }

        head_file_contents += "\n";
        head_file_contents += "private:\n";
        head_file_contents += "    GDBusConnection *conn;\n";
        head_file_contents += "};\n";
        head_file_contents += "\n";
    }



    head_file_contents += "#endif\n";

    cout << head_file_contents << endl;
    ofstream h_file;
    h_file.open(outfile, ios_base::out | ios_base::trunc);  //删除文件重写
    stringstream ss;
    ss << head_file_contents;
    h_file << ss.str() << endl;
    h_file.close();
    cout << "generate server header file done !" << endl;

    return 0;
}

static int generate_server_common_head(vector<string>& xmlfiles)
{
    const char *outfile = "server_common.h";
    string head_file_contents = "";

    head_file_contents += "#ifndef SERVER_COMMON_H\n";
    head_file_contents += "#define SERVER_COMMON_H\n";

    for(int i =0; i < xmlfiles.size();i++)
    {
        string filename = xmlfiles[i].substr(0,xmlfiles[i].size()-4);
        //    if (!filename.empty())
        //        filename[0] = toupper(filename[0]);
        //    string classname = filename;


        transform(filename.begin(),filename.end(),filename.begin(),::tolower);
        string classheaderfile = filename + ".h";

        head_file_contents += "#include \""+ classheaderfile +"\"\n";

    }


    head_file_contents += "#include <iostream>\n";
    head_file_contents += "\n";
    head_file_contents += "using namespace std;\n";

    head_file_contents += "static GDBusNodeInfo *introspection_data = NULL;\n";
    head_file_contents += "static GDBusConnection *c;\n";
    head_file_contents += "static  char introspection_xml[1024*100] = {0};\n";
    head_file_contents += "\n";



    head_file_contents += "static void loadXML(const char* path)\n";
    head_file_contents += "{\n";
    head_file_contents += "    std::ifstream file(path, std::ios::binary);\n";
    head_file_contents += "    file.seekg(0, std::ios::end);\n";
    head_file_contents += "    int size = file.tellg();\n";
    head_file_contents += "    char* data = new char[size];\n";
    head_file_contents += "\n";
    head_file_contents += "    file.seekg(0, std::ios::beg);\n";
    head_file_contents += "    file.read(data, size);\n";
    head_file_contents += "    file.close();\n";
    head_file_contents += "    g_print(\"xml contents: %s  size: %d, len: %d\\n\",data,size,strlen(data));\n";
    head_file_contents += "    memcpy((char*)introspection_xml,data,size);\n";
    head_file_contents += "    delete[] data;\n";
    head_file_contents += "}\n";
    head_file_contents += "\n";





    head_file_contents += "struct Server_Pointers{\n";
    const char* objPath = NULL;
    vector<string> interfaces;
    XMLDocument xmldoc;
    xmldoc.Clear();
    for(int i = 0 ; i < xmlfiles.size(); i++)
    {
        xmldoc.LoadFile(xmlfiles[i].c_str());
        XMLElement* root = xmldoc.RootElement();
        if(root != NULL)
        {
            objPath = root->Attribute("name");
            //cout << "objpath : " << objPath << endl;
        }else{
            printf("xml parse failed!\n");
            return -1;
        }
        XMLNode* pInterface  = NULL;
        for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
        {
            if(pInterface->ToElement()->Attribute("name") == NULL)
            {
                cout << "ERR: Interface without a name,please check it!" << endl;
                return -3;
            }
            if(pInterface->ToElement()->Attribute("name") != NULL)
            {
                head_file_contents += "    ";
                head_file_contents += create_sever_class_name(pInterface->ToElement()->Attribute("name"));
                head_file_contents += " *" + get_interface_suffix(pInterface->ToElement()->Attribute("name")) +";\n";
            }

        }

    }

    head_file_contents += "};\n";
    head_file_contents += "\n";


    head_file_contents += "struct Dbus_User_Data{\n";
    head_file_contents += "    const char *xml;;\n";
    head_file_contents += "    int interfaceCount;\n";
    head_file_contents += "    const char* path;\n";
    head_file_contents += "    Server_Pointers *p;\n";
    head_file_contents += "};\n";
    head_file_contents += "\n";



    //    XMLNode* pInterface  = NULL;
    //    XMLNode *pMethod_Signal = NULL;
    //    XMLNode *pArgs = NULL;

    //    for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
    //    {
    //        head_file_contents += "    ";
    //        head_file_contents += create_sever_class_name(pInterface->ToElement()->Attribute("name"));
    //        head_file_contents += " *" + get_interface_suffix(pInterface->ToElement()->Attribute("name")) +";\n";

    //    }



    head_file_contents += "static void\n";
    head_file_contents += "handle_method_call (GDBusConnection       *connection,\n";
    head_file_contents += "                    const gchar           *sender,\n";
    head_file_contents += "                    const gchar           *object_path,\n";
    head_file_contents += "                    const gchar           *interface_name,\n";
    head_file_contents += "                    const gchar           *method_name,\n";
    head_file_contents += "                    GVariant              *parameters,\n";
    head_file_contents += "                    GDBusMethodInvocation *invocation,\n";
    head_file_contents += "                    gpointer               user_data)\n";
    head_file_contents += "{\n";
    head_file_contents += "    Server_Pointers *data = (Server_Pointers*)((Dbus_User_Data* )user_data)->p;\n";
    head_file_contents += "\n";


    for(int i = 0 ; i < xmlfiles.size(); i++)
    {
        xmldoc.Clear();
        xmldoc.LoadFile(xmlfiles[i].c_str());
        XMLElement* root = xmldoc.RootElement();
        if(root != NULL)
        {
            objPath = root->Attribute("name");
            cout << "objpath : " << objPath << endl;
        }else{
            printf("xml parse failed!\n");
            return -1;
        }
        XMLNode* pInterface  = NULL;
        XMLNode *pMethod_Signal = NULL;
        XMLNode *pArgs = NULL;
        for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
        {
            if(pInterface->ToElement()->Attribute("name") != NULL)
            {
                head_file_contents += "    if((g_strcmp0 (object_path, \"";
                head_file_contents += string(objPath);
                head_file_contents += "\") == 0) && ";
                head_file_contents += "(g_strcmp0 (interface_name, \"";
                head_file_contents += pInterface->ToElement()->Attribute("name");
                head_file_contents += "\") == 0))\n";
                head_file_contents += "    {\n";
                head_file_contents += "        ";
                head_file_contents += create_sever_class_name(pInterface->ToElement()->Attribute("name"));
                head_file_contents += " *" + get_interface_suffix(pInterface->ToElement()->Attribute("name"));
                head_file_contents += " = data->" + get_interface_suffix(pInterface->ToElement()->Attribute("name")) + ";\n";
                head_file_contents += "        if(" + get_interface_suffix(pInterface->ToElement()->Attribute("name"));
                head_file_contents += " == NULL)\n";
                head_file_contents += "        {\n";
                head_file_contents += "            g_print(\"server error!\\n\");\n";
                head_file_contents += "            g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,\n";
                head_file_contents += "                                                         G_DBUS_ERROR_UNKNOWN_OBJECT,\n";
                head_file_contents += "                                                         \"The Server object has not implemented yet\");\n";
                head_file_contents += "            return;\n";
                head_file_contents += "        }\n";
                head_file_contents += "\n";

                for(pMethod_Signal = pInterface->FirstChildElement(); pMethod_Signal; pMethod_Signal = pMethod_Signal->NextSiblingElement())
                {
                    if(pMethod_Signal->ToElement()->Value() == NULL)
                    {
                        cout << "ERR: method or signal without type,please check it!" << endl;
                        return -3;
                    }

                    if(pMethod_Signal->ToElement()->Attribute("name") == NULL)
                    {
                        cout << "ERR: method or signal without a name,please check it!" << endl;
                        return -3;
                    }

                    string type = pMethod_Signal->ToElement()->Value();
                    string method_signal_name = pMethod_Signal->ToElement()->Attribute("name");
                    string arg_name,arg_type,dir,out_type,out_type_origin,arg_list,arg_list_origin,arg_list_refer,arg_types_in;

                    if(type.compare("method") == 0)//handle method
                    {
                        head_file_contents += "        if(g_strcmp0 (method_name, \"";
                        head_file_contents +=  method_signal_name;
                        head_file_contents +=        "\") == 0){\n";

                        cout << "method name: " << method_signal_name << endl;
                        for(pArgs = pMethod_Signal->FirstChildElement(); pArgs; pArgs = pArgs->NextSiblingElement())
                        {
                            if(pArgs->ToElement()->Attribute("name") != NULL)
                                arg_name = pArgs->ToElement()->Attribute("name");

                            if(pArgs->ToElement()->Attribute("type") != NULL)
                                arg_type = pArgs->ToElement()->Attribute("type");

                            cout << "arg type: " << arg_type << endl;

                            //                            if(pArgs->ToElement()->Attribute("direction") != NULL)
                            //                            {
                            if(pArgs->ToElement()->Attribute("direction") != NULL)
                                dir = string(pArgs->ToElement()->Attribute("direction"));
                            else
                                dir = string("");

                            cout << "dir is " << dir.compare("out") << endl;
                            if((dir.compare("out") == 0))
                            {
                                out_type = getRealType(arg_type);
                                out_type_origin = pArgs->ToElement()->Attribute("type");

                            }else{
                                arg_list += "            ";
                                arg_list += getRealType(arg_type);
                                arg_list += arg_name;
                                arg_list += ";\n";

                                arg_list_origin += arg_name;
                                arg_list_origin += ",";

                                arg_list_refer += ",&";
                                arg_list_refer += arg_name;


                                arg_types_in += pArgs->ToElement()->Attribute("type");
                                cout << "arg_types_in: " << arg_types_in << endl;

                            }



                        }

                        if(arg_list.size() > 0)
                            arg_list.pop_back();
                        else
                            arg_list = string("");

                        if(arg_list_origin.size() > 0)
                            arg_list_origin.pop_back();
                        else
                            arg_list_origin = string("");


                        head_file_contents += arg_list;
                        head_file_contents += "\n";
                        head_file_contents += "            g_variant_get(parameters, \"(";
                        head_file_contents += arg_types_in;
                        head_file_contents += ")\"";
                        head_file_contents += arg_list_refer;
                        head_file_contents += ");\n";

                        head_file_contents += "            ";
                        head_file_contents += out_type;
                        head_file_contents += "response = ";
                        head_file_contents += get_interface_suffix(pInterface->ToElement()->Attribute("name")) +"->";
                        head_file_contents += method_signal_name;
                        head_file_contents += "(";
                        head_file_contents += arg_list_origin;
                        head_file_contents += ");\n";
                        head_file_contents += "            g_dbus_method_invocation_return_value (invocation,\n";
                        if(out_type_origin.compare("av") != 0)
                        {
                            head_file_contents += "            g_variant_new (\"(";
                            head_file_contents += out_type_origin;
                            head_file_contents += ")\",response));\n";
                        }else{
                            head_file_contents += "            response);\n";
                        }
                        head_file_contents += "            return;\n";
                        head_file_contents += "        }\n";
                        head_file_contents += "     \n";

                    }
                }

                head_file_contents += "    }\n";
                head_file_contents += "\n";
            }
        }

    }

    head_file_contents += "}\n";
    head_file_contents += "\n";
    head_file_contents += "static const GDBusInterfaceVTable interface_vtable =\n";
    head_file_contents += "{\n";
    head_file_contents += "    handle_method_call,\n";
    head_file_contents += "    NULL,\n";
    head_file_contents += "    NULL,\n";
    head_file_contents += "    {0}\n";
    head_file_contents += "};\n";
    head_file_contents += "\n";
    //    head_file_contents += "static void\n";
    //    head_file_contents += "on_bus_acquired (GDBusConnection *connection,\n";
    //    head_file_contents += "                 const gchar     *name,\n";
    //    head_file_contents += "                 gpointer         user_data)\n";
    //    head_file_contents += "{\n";
    //    head_file_contents += "    c = connection;\n";
    //    head_file_contents += "    Dbus_User_Data *data = (Dbus_User_Data*)user_data;\n";
    //    head_file_contents += "    if(data == NULL)\n";
    //    head_file_contents += "    {\n";
    //    head_file_contents += "        g_print(\"server error!\\n\");\n";
    //    head_file_contents += "        return;\n";
    //    head_file_contents += "    }\n";

    //    for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
    //    {
    //        head_file_contents += "    data->" + get_interface_suffix(pInterface->ToElement()->Attribute("name"));
    //        head_file_contents += "->setConnection(c);\n";
    //    }

    //    head_file_contents += "    g_print(\"count : %d\\n\",data->interfaceCount);\n";
    //    head_file_contents += "\n";
    //    head_file_contents += "    for(gint32 i = 0; i < data->interfaceCount; i++)\n";
    //    head_file_contents += "    {\n";
    //    head_file_contents += "        guint registration_id;\n";
    //    head_file_contents += "        registration_id = g_dbus_connection_register_object (connection,\n";
    //    head_file_contents += "                                                             \""+ string(objPath) +"\",\n";
    //    head_file_contents += "                                                             introspection_data->interfaces[i],\n";
    //    head_file_contents += "                                                             &interface_vtable,\n";
    //    head_file_contents += "                                                             user_data,\n";
    //    head_file_contents += "                                                             NULL,\n";
    //    head_file_contents += "                                                             NULL);\n";
    //    head_file_contents += "        g_assert (registration_id > 0);\n";
    //    head_file_contents += "    }\n";
    //    head_file_contents += "}\n";
    //    head_file_contents += "\n";
    head_file_contents += "static void\n";
    head_file_contents += "on_name_acquired (GDBusConnection *connection,\n";
    head_file_contents += "                  const gchar     *name,\n";
    head_file_contents += "                  gpointer         user_data)\n";
    head_file_contents += "{\n";
    head_file_contents += "}\n";
    head_file_contents += "\n";
    head_file_contents += "static void\n";
    head_file_contents += "on_name_lost (GDBusConnection *connection,\n";
    head_file_contents += "              const gchar     *name,\n";
    head_file_contents += "              gpointer         user_data)\n";
    head_file_contents += "{\n";
    head_file_contents += "    exit (1);\n";
    head_file_contents += "}\n";
    head_file_contents += "\n";
    head_file_contents += "static void init(const char* server_name,Server_Pointers *p)\n";
    head_file_contents += "{\n";
    //    head_file_contents += "    char* xmlPath = \"" + string(xmlfile) + "\";\n";
    head_file_contents += "    guint owner_id;\n";
    head_file_contents += "    GMainLoop *loop;\n";
    //    head_file_contents += "    loadXML(xmlPath);\n";
    //    head_file_contents += "\n";
    //    head_file_contents += "    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);\n";
    //    head_file_contents += "    g_assert (introspection_data != NULL);\n";
    head_file_contents += "\n";
    head_file_contents += "    owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,\n";
    head_file_contents += "                               server_name,\n";
    head_file_contents += "                               G_BUS_NAME_OWNER_FLAGS_NONE,\n";
    head_file_contents += "                               NULL,\n";
    head_file_contents += "                               on_name_acquired,\n";
    head_file_contents += "                               on_name_lost,\n";
    head_file_contents += "                               p,\n";
    head_file_contents += "                               NULL);\n";
    head_file_contents += "    c = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);\n";

    for(int i = 0 ; i < xmlfiles.size(); i++)
    {
        xmldoc.Clear();
        xmldoc.LoadFile(xmlfiles[i].c_str());
        XMLElement* root = xmldoc.RootElement();
        if(root != NULL)
        {
            objPath = root->Attribute("name");
            //cout << "objpath : " << objPath << endl;
        }else{
            printf("xml parse failed!\n");
            return -1;
        }
        XMLNode* pInterface  = NULL;
        for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
        {
            if(pInterface->ToElement()->Attribute("name") != NULL)
            {
                head_file_contents += "    if(p->" + get_interface_suffix(pInterface->ToElement()->Attribute("name"));
                head_file_contents += " != NULL)\n";
                head_file_contents += "        p->" + get_interface_suffix(pInterface->ToElement()->Attribute("name"));
                head_file_contents += "->setConnection(c);\n";
            }
        }

    }

    head_file_contents += "\n";

    int xmlCnt = 0;
    string user_data_list = "";
    for(int i = 0 ; i < xmlfiles.size(); i++)
    {
        xmldoc.Clear();
        xmldoc.LoadFile(xmlfiles[i].c_str());
        XMLElement* root = xmldoc.RootElement();
        if(root != NULL)
        {
            objPath = root->Attribute("name");
            //cout << "objpath : " << objPath << endl;
        }else{
            printf("xml parse failed!\n");
            return -1;
        }
        head_file_contents += "    Dbus_User_Data user_data" + to_string(i) + ";\n";
        head_file_contents += "    user_data" + to_string(i) + ".xml = \"" + xmlfiles[i] + "\";\n";
        head_file_contents += "    user_data" + to_string(i) + ".path = \"" + string(objPath) + "\";\n";
        head_file_contents += "    user_data" + to_string(i) + ".interfaceCount = " + to_string(interface_count(xmlfiles[i].c_str())) + ";\n";
        head_file_contents += "    user_data" + to_string(i) + ".p = p;\n";
        user_data_list += "&user_data" + to_string(i) + ",";
        xmlCnt++;
    }
    if(user_data_list.size() > 0)
        user_data_list.pop_back();

    head_file_contents += "\n";
    head_file_contents += "    Dbus_User_Data *user_data[" + to_string(xmlCnt) + "] = {" + user_data_list + "};\n";
    head_file_contents += "\n";
    head_file_contents += "    for(int i= 0 ; i < " + to_string(xmlCnt) +";i++)\n";
    head_file_contents += "    {\n";
    head_file_contents += "        loadXML(user_data[i]->xml);\n";
    head_file_contents += "        if(introspection_data != NULL)\n";
    head_file_contents += "        {\n";
    head_file_contents += "           g_dbus_node_info_unref (introspection_data);\n";
    head_file_contents += "           introspection_data = NULL;\n";
    head_file_contents += "        }\n";
    head_file_contents += "\n";
    head_file_contents += "        introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);\n";
    head_file_contents += "        g_assert (introspection_data != NULL);\n";
    head_file_contents += "\n";
    head_file_contents += "        Dbus_User_Data *data = user_data[i];\n";
    head_file_contents += "        for(gint32 j = 0; j < data->interfaceCount; j++)\n";
    head_file_contents += "        {\n";
    head_file_contents += "            g_print(\"interface : %s \\n\",introspection_data->interfaces[j]->name);\n";
    head_file_contents += "            guint registration_id;\n";
    head_file_contents += "\n";
    head_file_contents += "            registration_id = g_dbus_connection_register_object (c,\n";
    head_file_contents += "                                                                 data->path,\n";
    head_file_contents += "                                                                 introspection_data->interfaces[j],\n";
    head_file_contents += "                                                                 &interface_vtable,\n";
    head_file_contents += "                                                                 user_data[i],\n";
    head_file_contents += "                                                                 NULL,\n";
    head_file_contents += "                                                                 NULL);\n";
    head_file_contents += "            g_assert (registration_id > 0);\n";
    head_file_contents += "        }\n";
    head_file_contents += "    }\n";
    head_file_contents += "\n";

    head_file_contents += "    loop = g_main_loop_new (NULL, FALSE);\n";
    head_file_contents += "    g_print(\"Hello GDbus Server started...\\n\");\n";
    head_file_contents += "\n";
    head_file_contents += "    g_main_loop_run (loop);\n";
    head_file_contents += "\n";
    head_file_contents += "    g_bus_unown_name (owner_id);\n";
    head_file_contents += "\n";
    head_file_contents += "    g_dbus_node_info_unref (introspection_data);\n";
    head_file_contents += "}\n";
    head_file_contents += "\n";
    //    for(pInterface=root->FirstChildElement();pInterface;pInterface=pInterface->NextSiblingElement())
    //    {
    //        head_file_contents += "REGISTER(";
    //        head_file_contents += create_sever_class_name(pInterface->ToElement()->Attribute("name"));
    //        head_file_contents += ");\n";
    //    }
    head_file_contents += "#endif\n";
    head_file_contents += "\n";

    cout << head_file_contents << endl;

    ofstream h_file;
    h_file.open(outfile, ios_base::out | ios_base::trunc);  //删除文件重写
    stringstream ss;
    ss << head_file_contents;
    h_file << ss.str() << endl;
    h_file.close();
    cout << "generate server common header file done !" << endl;

    return 0;
}

static int generate_server_files(vector<string>& xmlfiles)
{
    if(generate_server_common_head(xmlfiles) != 0)
    {
        cout << "generate server common head failed !" << endl;
        return -1;
    }

    //    if(generate_class_factory() != 0)
    //    {
    //        cout << "generate class factory failed !" << endl;
    //        return -2;
    //    }

    for(int i = 0; i < xmlfiles.size() ; i++)
    {
        string filename = xmlfiles[i].substr(0,xmlfiles[i].size()-4);
        if(generate_server_head_file(xmlfiles[i].c_str(),filename) != 0)
        {
            cout << "generate server head file failed !" << endl;
            return -3;
        }

        if(generate_server_cpp_file(xmlfiles[i].c_str(),filename) != 0)
        {
            cout << "generate server cpp file failed !" << endl;
            return -4;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{

    cout << "Create Client files Command: ./gdbusxml2cpp -i xxx.xml -c" << endl;
    cout << "Create Server files Command: ./gdbusxml2cpp -i xxx.xml -s " << endl;



    //generate_interface("server.xml",string("server_interface"));

    int c;
    //定义长参数选项，如--file
    static struct option long_options[] =
    {
    {"help", no_argument,NULL,'h'},
    {"input", required_argument,NULL,'i'},
    {"output client",no_argument, NULL,'c'},
    {"output server",no_argument, NULL,'s'}
};

    string filename = "";
    vector<string> xmls;

    while(1)
    {
        int opt_index = 0;
        //参数解析方法，重点
        c = getopt_long(argc, argv,"hi:cs", long_options,&opt_index);

        if(-1 == c)
        {
            break;
        }

        switch(c)
        {
        case 'i':
            cout << "input xml: " << optarg << endl;
            parse_include_xml(optarg,xmls);
            break;
        case 'c'://generate client file
        {
            cout << "size:" << xmls.size() << endl;
            vector<string> filenames;
            for(int i = 0; i < xmls.size(); i++)
            {
                if(xmls[i].empty())
                {
                    cout << "input or output file is empty, please check filenames" << endl;
                    return 0;
                }

                //xmls[i].erase(xmls[i].end()-4,xmls[i].end());
                filename = xmls[i].substr(0,xmls[i].size()-4);
                filename += "_interface";
                cout << "out file name : " << filename << endl;
                filenames.push_back(filename);
                generate_interface(xmls[i].c_str(),filename);
            }
            generate_client_common_file(filenames);
        }
            break;
        case 's':{
            cout << "generate server files" << endl;
            generate_server_files(xmls);

        }break;
        case 'h':
            cout << "Create Client files Command: ./gdbusxml2cpp -i xxx.xml -c" << endl;
            cout << "Create Server files Command: ./gdbusxml2cpp -i xxx.xml -s " << endl;
            break;
        default:
            cout << "Illegal parameters!!" << endl;

            break;
        }

    }



    return 0;
}
