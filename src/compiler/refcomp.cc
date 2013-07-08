#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>

namespace gp = google::protobuf;
namespace gpc = google::protobuf::compiler;

std::ofstream xx_;
std::ofstream xs_;
std::ofstream xc_;
std::map<std::string, int> proc_;
std::string dir_;

std::string refcomp_type_name(const google::protobuf::FieldDescriptor* f, bool nb) {
    switch (f->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        return nb ? "str" : "std::string";
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        assert(0 && "Nested message is not supported yet");
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        return "int32_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        return "int64_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        return "uint32_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        return "uint64_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        return "double";
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        return "float";
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        return "bool";
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        return f->enum_type()->name();
    default:
        assert(0 && "Unknown type");
    };
}

struct nbcg: public gpc::CodeGenerator {
    bool Generate(const gp::FileDescriptor* file, const std::string& parameter,
                  gpc::GeneratorContext*, std::string* error) const;
  private:
    void generateEnum(const gp::EnumDescriptor* d) const;
    void generateProcNumber(const gp::FileDescriptor* file) const;
    void generateRequestAnalyzer(const gp::FileDescriptor* file) const;
    void generateMessage(const gp::Descriptor* d) const;
    void generateMessage(const gp::Descriptor* d, bool nb) const;

    void generateXC(const gp::FileDescriptor* file) const;
    void generateXS(const gp::FileDescriptor* file) const;

};

void nbcg::generateXC(const gp::FileDescriptor* file) const {
    xc_.open(dir_ + "/fastrpc_proto_client.hh");
    xc_ << "#ifndef REFCOMP_FASTRPC_CLIENT_HH\n"
        << "#define REFCOMP_FASTRPC_CLIENT_HH 1\n"
        << "#include \"rpc/async_rpcc_helper.hh\"\n\n"
        << "namespace " << file->package() << "{\n\n"
        << "template <typename SELF>\n"
        << "struct make_callable {\n";

    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xc_ << "    template <void (SELF::*method)(" << m->output_type()->name() << "&)>\n"
                << "    inline rpc::make_unary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name() 
                << ", method> make_call() {\n"
                << "        return rpc::make_unary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name()
                << ", method>((SELF*)this);\n"
                << "    }\n";

            xc_ << "    template <void (SELF::*method)(" << m->output_type()->name() << "&)>\n"
                << "    inline rpc::make_binary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name() 
                << ", method> make_call() {\n"
                << "        return rpc::make_binary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name()
                << ", method>((SELF*)this);\n"
                << "    }\n";
        }
    }
    xc_ << "};\n\n";
    xc_ << "}; // namespace " << file->package() << "\n"
        << "#endif\n";
}

std::string up(const std::string& s) {
    std::string up = s;
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    return up;
}

void nbcg::generateXS(const gp::FileDescriptor* file) const {
    xs_.open(dir_ + "/fastrpc_proto_server.hh");
    xs_ << "#ifndef REFCOMP_FASTRPC_SERVER_HH\n"
        << "#define REFCOMP_FASTRPC_SERVER_HH 1\n"
        << "#include \"fastrpc_proto.hh\"\n"
        << "#include \"rpc/grequest.hh\"\n"
        << "#include \"rpc/rpc_server_base.hh\"\n\n"
        << "namespace " << file->package() << "{\n\n";
    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        xs_ << "template <bool NB_DEFAULT = false";
        for (int j = 0; j < s->method_count(); ++j)
            xs_ << ", bool NB_" << up(s->method(j)->name()) << " = NB_DEFAULT";
        xs_ << ">\n";
        
        xs_ << "struct " << s->name() << "Interface : public rpc::rpc_server_base {\n"
            << "    template <bool NB, ProcNumber PROC>\n"
            << "    void report_failure() {\n"
            << "        if (NB){\n"
            << "            std::cerr << \"Please override non-blocking version of \" << ProcNumber_name(PROC) << std::endl;\n"
            << "            assert(0);\n"
            << "        } else {\n"
            << "            std::cerr << \"Please override blocking version of \" << ProcNumber_name(PROC) << std::endl;\n"
            << "            assert(0);\n"
            << "        }\n"
            << "    }\n";
        // stub
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "    virtual void " << m->name() << "(rpc::grequest<ProcNumber::" << m->name() << ">*, uint64_t) {\n"
                << "        report_failure<NB_" << up(m->name()) << ", ProcNumber::" << m->name() << ">();\n"
                << "    }\n";

            xs_ << "    virtual void " << m->name() << "(rpc::grequest<ProcNumber::" << m->name() << ", true>&, uint64_t) {\n"
                << "        report_failure<NB_" << up(m->name()) << ", ProcNumber::" << m->name() << ">();\n"
                << "    }\n";
        }

        // proclist
        xs_ << "    std::vector<int> proclist() const {\n"
            << "        ProcNumber pl[] = {";
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "ProcNumber::" << m->name();
            if (j < s->method_count() - 1)
                xs_ << ", ";
        }
        xs_ << "};\n";

        xs_ << "        return std::vector<int>(pl, pl + " << proc_.size() << ");\n"
            << "    }\n";
        // dispatch
        xs_ << "    void dispatch(rpc::parser& p, rpc::async_rpcc* c, uint64_t now) {\n"
            << "       rpc::rpc_header* h = p.header<rpc::rpc_header>();\n"
            << "       switch (h->proc_) {\n";
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "        case ProcNumber::" << m->name() << ":\n"
                << "            if (NB_" << up(m->name()) << ") {\n"
                << "                rpc::grequest_remote<ProcNumber::" << m->name() << ", true> q(h->seq_, c);\n"
                << "                p.parse_message(q.req_);\n"
                << "                " << m->name() << "(q, now);\n"
                << "            } else {\n"
                << "                auto q = new rpc::grequest_remote<ProcNumber::" << m->name() << ", false>(h->seq_, c);\n"
                << "                p.parse_message(q->req_);\n"
                << "                " << m->name() << "(q, now);\n"
                << "            }break;\n";
        };
        xs_ << "        default:\n"
            << "            assert(0 && \"Unknown RPC\");\n"
            << "        }\n"
            << "    }\n";

        xs_ << "}; // " << s->name() << "Interface\n";
    }
    

    xs_ << "}; // namespace " << file->package() << "\n"
        << "#endif\n";
}

bool nbcg::Generate(const gp::FileDescriptor* file, const std::string& parameter,
                    gpc::GeneratorContext*, std::string* error) const {
    dir_ = parameter;
    xx_.open(dir_ + "/fastrpc_proto.hh");
    xx_ << "#ifndef REFCOMP_FASTRPC_HH\n"
        << "#define REFCOMP_FASTRPC_HH 1\n"
        << "#include <inttypes.h>\n"
        << "#include <iostream>\n"
        << "#include <assert.h>\n"
        << "#include \"compiler/stream_parser.hh\"\n"
        << "\n";

    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            assert(proc_.find(m->name()) == proc_.end() && "method name must be globally unique");
            size_t index = proc_.size();
            proc_[m->name()] = index;
        }
    }
    for (int i = 0; i < file->enum_type_count(); ++i)
        generateEnum(file->enum_type(i));

    generateProcNumber(file);

    for (int i = 0; i < file->message_type_count(); ++i)
        generateMessage(file->message_type(i));

    generateRequestAnalyzer(file);

    xx_ << "#endif\n";

    generateXC(file);
    generateXS(file);
    return true;
}

void nbcg::generateRequestAnalyzer(const gp::FileDescriptor* file) const {
    xx_ << "namespace rpc {\n"
        << "template <uint32_t PROC, bool NB> struct analyze_grequest{};\n\n";

    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xx_ << "template<> struct analyze_grequest<" << file->package() 
                << "::ProcNumber::" << m->name() << ", true> {\n"
                << "    typedef " << file->package() << "::nb_" << m->input_type()->name() << " request_type;\n"
                << "    typedef " << file->package() << "::nb_" << m->output_type()->name() << " reply_type;\n"
                << "};\n\n";

            xx_ << "template<> struct analyze_grequest<" << file->package() 
                << "::ProcNumber::" << m->name() << ", false> {\n"
                << "    typedef " << file->package() << "::" << m->input_type()->name() << " request_type;\n"
                << "    typedef " << file->package() << "::" << m->output_type()->name() << " reply_type;\n"
                << "};\n\n";
        }
    }

    xx_ << "struct app_param {\n"
        << "    typedef " << file->package() << "::ErrorCode ErrorCode;\n"
        << "    static constexpr uint32_t nproc = " << file->package() << "::ProcNumber::nproc;\n"
        << "};\n";

    xx_ << "}; // namespace rpc\n";
}

void nbcg::generateProcNumber(const gp::FileDescriptor* file) const {
    xx_ << "namespace " << file->package() << "{\n"
        << "enum ProcNumber {\n";
    for (auto p : proc_)
        xx_ << "    " << p.first << " = " << p.second << ",\n";
    xx_ << "    nproc = " << proc_.size() << ",\n";
    xx_ << "}; // enum \n";

    xx_ << "inline const char* ProcNumber_name(ProcNumber n) {\n"
        << "    switch(n) {\n";
    for (auto p : proc_)
        xx_ << "    case ProcNumber::" << p.first << ":\n"
            << "        return \"" << p.first << "\";\n";
    xx_ << "    default:\n"
        << "        assert(0 && \"Bad value\");\n"
        << "    }\n"
        << "}\n";

    xx_ << "}; // namespace\n";

}

void nbcg::generateEnum(const gp::EnumDescriptor* d) const {
    xx_ << "namespace " << d->file()->package() << "{\n"
        << "enum " << d->name() << "{\n";
    for (int i = 0; i < d->value_count(); ++i) {
        auto vd = d->value(i);
        xx_ << "    " << vd->name() << " = " << vd->number() << ",\n";
    }
    xx_ << "}; // enum \n";

    xx_ << "inline const char* " << d->name() << "_name(" << d->name() << " n) {\n"
        << "    switch(n) {\n";
    for (int i = 0; i < d->value_count(); ++i) {
        auto vd = d->value(i);
        xx_ << "    case " << d->name() << "::"<< vd->name() << ":\n"
            << "        return \"" << vd->name() << "\";\n";
    }
    xx_ << "    default:\n"
        << "        assert(0 && \"Bad value\");\n"
        << "    }\n"
        << "}\n";

    xx_ << "}; // namespace\n";
}

void nbcg::generateMessage(const gp::Descriptor* d) const {
    generateMessage(d, true);
    generateMessage(d, false);
}

void nbcg::generateMessage(const gp::Descriptor* d, bool nb) const {
    std::string className = (nb ? "nb_" : "") + d->name();
    xx_ << "namespace " << d->file()->package() << "{\n"
        << "class " << className << "{\n"
        << "  public:\n";
    
    // constructor
    xx_  << "    " << className << "()";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (i == 0)
            xx_ << " : ";
        xx_ << f->name() << "_()";
        if (i < d->field_count() - 1)
            xx_ << ", ";
    }
    xx_ << "{} // constructor\n";

    // SerializeToArray
    xx_ << "    bool SerializeToArray(uint8_t* s, size_t) {\n"
        << "        stream_unparser su(s);\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        xx_ << "        su.unparse(" << f->name() << "_);\n";
    }
    xx_ << "        return true;\n";
    xx_ << "    }\n";
    
    // ParseFromArray
    xx_ << "    void ParseFromArray(const void* data, size_t size) {\n"
        << "        stream_parser sp(data, size);\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        xx_ << "        sp.parse(" << f->name() << "_);\n";
    }
    xx_ << "    }\n";

    // ByteSize
    xx_ << "    size_t ByteSize() {\n"
        << "        size_t size = 0;\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        xx_ << "        size += stream_unparser::bytecount(" << f->name() << "_);\n";
    }
    xx_ << "        return size;\n"
        << "    }\n";

    // Getter/Setter
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        xx_ << "    const " << refcomp_type_name(f, nb) << "& " << f->name() << "() const {\n"
            << "        return " << f->name() << "_;\n"
            << "    }\n";

        xx_ << "    void set_" << f->name() << "(const " << refcomp_type_name(f, nb) << "& v) {\n"
            << "        " << f->name() << "_ = v;\n"
            << "    }\n";

        xx_ << "    " << refcomp_type_name(f, nb) << "* mutable_" << f->name() << "() {\n"
            << "        return &" << f->name() << "_;\n"
            << "    }\n";
    }

    // Storage
    xx_ << "  private:\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        xx_ << "    " << refcomp_type_name(f, nb) << " " << f->name() << "_;\n";
    }

    xx_ << "};\n"
        << "}; //namespace " << d->file()->package() << "\n";
}

int main(int argc, char* argv[]) {
    nbcg generator;
    return gpc::PluginMain(argc, argv, &generator);
}
