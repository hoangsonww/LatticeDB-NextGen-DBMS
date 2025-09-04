#include "storage.h"
#include <fstream>

static std::string escape(const std::string& s){
    std::string o; o.reserve(s.size());
    for(char c:s){ if(c=='\\'||c=='|'||c=='\n') {o.push_back('\\');} o.push_back(c); }
    return o;
}
static std::string unescape(const std::string& s){
    std::string o; o.reserve(s.size());
    bool esc=false;
    for(char c:s){
        if(!esc && c=='\\'){ esc=true; continue; }
        o.push_back(c); esc=false;
    }
    return o;
}

void Database::save(const std::string& path) const {
    std::ofstream ofs(path, std::ios::binary);
    ofs << "FORGEDB_SNAPSHOT_V1\n";
    ofs << "TX " << next_tx << "\n";
    ofs << "TABLES " << data.size() << "\n";
    for (const auto& [tname, td] : data) {
        ofs << "T " << tname << "\n";
        ofs << "C " << td.def.cols.size() << "\n";
        for (const auto& c : td.def.cols) {
            ofs << "COL " << c.name << "|" << (int)c.type << "|" << (int)c.merge.kind
                << "|" << c.merge.minv << "|" << c.merge.maxv << "|" << c.vector_dim
                << "|" << (c.primary_key?1:0) << "\n";
        }
        ofs << "V " << td.versions.size() << "\n";
        for (const auto& rv : td.versions) {
            ofs << "R " << escape(rv.row_id) << "|" << rv.tx_from << "|" << rv.tx_to
                << "|" << escape(rv.valid_from) << "|" << escape(rv.valid_to) << "\n";
            ofs << "D " << rv.data.size() << "\n";
            for (const auto& val : rv.data) {
                if (std::holds_alternative<std::monostate>(val.v)) ofs << "N|\n";
                else if (std::holds_alternative<int64_t>(val.v)) ofs << "I|" << std::get<int64_t>(val.v) << "\n";
                else if (std::holds_alternative<double>(val.v)) ofs << "F|" << std::get<double>(val.v) << "\n";
                else if (std::holds_alternative<std::string>(val.v)) ofs << "S|" << escape(std::get<std::string>(val.v)) << "\n";
                else if (std::holds_alternative<std::set<std::string>>(val.v)) {
                    ofs << "G|";
                    bool first=true;
                    for (auto& s : std::get<std::set<std::string>>(val.v)) {
                        if(!first) ofs<<",";
                        ofs << escape(s); first=false;
                    }
                    ofs << "\n";
                } else if (std::holds_alternative<std::vector<double>>(val.v)) {
                    ofs << "V|";
                    const auto& ar = std::get<std::vector<double>>(val.v);
                    for (size_t i=0;i<ar.size();++i){ if(i) ofs<<","; ofs<<ar[i]; }
                    ofs << "\n";
                }
            }
        }
    }
}

bool Database::load(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.good()) return false;
    std::string line;
    std::getline(ifs, line); if (line!="FORGEDB_SNAPSHOT_V1") return false;
    std::getline(ifs, line); // TX
    {
        auto sp = util::split(line, ' ');
        if (sp.size()==2) next_tx = std::stoll(sp[1]);
    }
    std::getline(ifs, line); // TABLES
    size_t nt=0; { auto sp=util::split(line,' '); if(sp.size()==2) nt=std::stoull(sp[1]); }
    data.clear(); catalog.tables.clear();
    for (size_t t=0;t<nt;++t) {
        std::getline(ifs, line); // T name
        auto sp=util::split(line,' ');
        std::string tname = sp.size()==2? sp[1] : "UNKNOWN";
        TableData td;
        td.def.name = tname;

        std::getline(ifs, line); // C count
        size_t nc=0; { auto sp2=util::split(line,' '); if(sp2.size()==2) nc=std::stoull(sp2[1]); }
        for (size_t i=0;i<nc;++i) {
            std::getline(ifs, line); // COL ...
            auto rest = line.substr(4);
            auto parts = util::split(rest, '|', true);
            ColumnDef c;
            c.name = parts[0];
            c.type = (ColumnType)std::stoi(parts[1]);
            c.merge.kind = (MergeSpec::Kind)std::stoi(parts[2]);
            c.merge.minv = std::stoll(parts[3]);
            c.merge.maxv = std::stoll(parts[4]);
            c.vector_dim = (size_t)std::stoull(parts[5]);
            c.primary_key = (parts[6]=="1");
            td.def.cols.push_back(c);
        }
        td.def.pk_index = -1; for (size_t i=0;i<td.def.cols.size();++i) if (td.def.cols[i].primary_key) td.def.pk_index=(int)i;

        std::getline(ifs, line); // V count
        size_t nv=0; { auto sp2=util::split(line,' '); if(sp2.size()==2) nv=std::stoull(sp2[1]); }
        for (size_t r=0;r<nv;++r) {
            std::getline(ifs, line); // R ...
            auto rest = line.substr(2);
            auto parts = util::split(rest, '|', true);
            RowVersion rv; rv.row_id = unescape(parts[0]); rv.tx_from=std::stoll(parts[1]); rv.tx_to=std::stoll(parts[2]);
            rv.valid_from = unescape(parts[3]); rv.valid_to = unescape(parts[4]);

            std::getline(ifs, line); // D n
            size_t nd=0; { auto sp2=util::split(line,' '); if (sp2.size()==2) nd=std::stoull(sp2[1]); }
            rv.data.resize(nd);
            for (size_t k=0;k<nd;++k) {
                std::getline(ifs, line);
                if (line.size()<2) continue;
                char tag = line[0];
                auto content = line.size()>2? line.substr(2) : "";
                if (tag=='N') rv.data[k] = Value::Null();
                else if (tag=='I') rv.data[k] = Value::I(std::stoll(content));
                else if (tag=='F') rv.data[k] = Value::D(std::stod(content));
                else if (tag=='S') rv.data[k] = Value::S(unescape(content));
                else if (tag=='G') {
                    std::set<std::string> ss;
                    for (auto& s : util::split(content, ',')) ss.insert(unescape(s));
                    rv.data[k] = Value::SET(ss);
                } else if (tag=='V') {
                    std::vector<double> ar;
                    for (auto& s : util::split(content, ',')) if(!s.empty()) ar.push_back(std::stod(s));
                    rv.data[k] = Value::VEC(ar);
                }
            }
            td.versions.push_back(std::move(rv));
        }
        catalog.add_table(td.def);
        data[tname] = std::move(td);
    }
    return true;
}
