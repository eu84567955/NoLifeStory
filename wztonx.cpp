//////////////////////////////////////////////////////////////////////////////
// NoLifeWzToNx - Part of the NoLifeStory project                           //
// Copyright (C) 2013 Peter Atashian                                        //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define VC_EXTRALEAN
#  define NOMINMAX
#  include <Windows.h>
#else
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

#include <iostream>
#include <fstream>
#include <codecvt>
#include <vector>
#include <deque>
#include <map>
#include <cstdint>
#include <unordered_map>
#include <algorithm>

namespace nl {
    typedef uint16_t strsize_t;
    typedef char char8_t;
    typedef uint32_t node_t;
    typedef uint32_t string_t;
    typedef uint32_t hash_t;
    typedef uint8_t key_t;
    typedef uint32_t dir_t;
    typedef uint32_t img_t;

    //Utility
    template <typename T> struct identity {
        T operator()(T const & v) const {
            return v;
        }
    };

    //File stuff
    namespace in {
        char const * base {};
        char const * offset {};
#ifdef _WIN32
        void * file_handle {};
        void * map_handle {};
        void open(std::string p) {
            file_handle = CreateFileA(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (file_handle == INVALID_HANDLE_VALUE) throw std::runtime_error {"Failed to open file " + p};
            map_handle = CreateFileMappingA(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (map_handle == nullptr) throw std::runtime_error {"Failed to create file mapping of file " + p};
            base = reinterpret_cast<char *>(MapViewOfFile(map_handle, FILE_MAP_READ, 0, 0, 0));
            if (base == nullptr) throw std::runtime_error {"Failed to map view of file " + p};
            offset = base;
        }
        void close() {
            UnmapViewOfFile(base);
            CloseHandle(map_handle);
            CloseHandle(file_handle);
        }
#else
        int file_handle {};
        size_t file_size {};
        void open(std::string p) {
            file_handle = ::open(p.c_str(), O_RDONLY);
            if (file_handle == -1) throw std::runtime_error {"Failed to open file " + p};
            struct stat finfo;
            if (fstat(file_handle, &finfo) == -1) throw std::runtime_error {"Failed to obtain file information of file " + p};
            file_size = finfo.st_size;
            base = reinterpret_cast<char const *>(mmap(nullptr, file_size, PROT_READ, MAP_SHARED, file_handle, 0));
            if (reinterpret_cast<intptr_t>(base) == -1) throw std::runtime_error {"Failed to create memory mapping of file " + p};
            offset = base;
        }
        void close() {
            munmap(const_cast<char *>(base), file_size);
            ::close(file_handle);
        }
#endif
        size_t tell() {
            return static_cast<size_t>(offset - base);
        }
        void seek(size_t n) {
            offset = base + n;
        }
        void skip(size_t n) {
            offset += n;
        }
        template <typename T> T read() {
            T v {*reinterpret_cast<T const *>(offset)};
            offset += sizeof(T);
            return v;
        }
        int32_t read_cint() {
            int8_t a {read<int8_t>()};
            return a != -128 ? a : read<int32_t>();
        }
    }
    namespace out {
        char * base {};
        char * offset {};
#ifdef _WIN32
        void * file_handle {};
        void * map_handle {};
        void open(std::string p, size_t size) {
            file_handle = CreateFileA(p.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
            if (file_handle == INVALID_HANDLE_VALUE) throw std::runtime_error {"Failed to open file " + p};
            map_handle = CreateFileMappingA(file_handle, nullptr, PAGE_READWRITE, size >> 32, size & 0xffffffff, nullptr);
            if (map_handle == nullptr) throw std::runtime_error {"Failed to create file mapping of file " + p};
            base = reinterpret_cast<char *>(MapViewOfFile(map_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));
            if (base == nullptr) throw std::runtime_error {"Failed to map view of file " + p};
            offset = base;
        }
        void close() {
            UnmapViewOfFile(base);
            CloseHandle(map_handle);
            CloseHandle(file_handle);
        }
#else
        int file_handle;
        size_t file_size;
        void open(std::string p, uint64_t size) {
            file_handle = ::open(p.c_str(), O_RDWR | O_CREAT | O_TRUNC);
            if (file_handle == -1) throw std::runtime_error {"Failed to open file " + p};
            file_size = size;
            base = reinterpret_cast<char *>(mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_handle, 0));
            if (reinterpret_cast<intptr_t>(base) == -1) throw std::runtime_error {"Failed to create memory mapping of file " + p};
            offset = base;
        }
        void close() {
            munmap(const_cast<char *>(base), file_size);
            ::close(file_handle);
        }
#endif
        size_t tell() {
            return static_cast<size_t>(offset - base);
        }
        void seek(size_t n) {
            offset = base + n;
        }
        void skip(size_t n) {
            offset += n;
        }
        template <typename T> void write(T const & v) {
            *reinterpret_cast<T *>(offset) = v;
            offset += sizeof(T);
        }
        void write(void * buf, size_t size) {
            memcpy(offset, buf, size);
            offset += size;
        }
    }

    //Memory allocation
    namespace alloc {
        char * buffer {nullptr};
        size_t remain {0};
        size_t const default_size {0x1000000};
        char * big(size_t size) {
            return new char[size];
        }
        void * small(size_t size) {
            if (size > remain) {
                buffer = big(default_size);
                remain = default_size;
            }
            char * r {buffer};
            buffer += size, remain -= size;
            return r;
        }
    }
    //Node stuff
#pragma pack(push, 1)
    struct node {
        enum class type : uint16_t {
            none = 0,
                integer = 1,
                real = 2,
                string = 3,
                vector = 4,
                bitmap = 5,
                audio = 6,
                uol = 7
        };
        node() : name {0}, children {0}, num {0}, data_type {type::none}, data {} {}
        uint32_t name;
        uint32_t children;
        uint16_t num;
        type data_type;
        union _data {
            _data() : integer {0} {}
            int64_t integer;
            double real;
            uint32_t string;
            int32_t vector[2];
            struct _bitmap {
                uint32_t id;
                uint16_t width;
                uint16_t height;
            } bitmap;
            struct _audio {
                uint32_t id;
                uint32_t length;
            } audio;
        } data;
    };
#pragma pack(pop)
    std::vector<node> nodes {1};
    std::vector<std::pair<node_t, node_t>> nodes_to_sort {};
    //String stuff
    struct string {
        char8_t * data;
        strsize_t size;
    };
    std::unordered_map<hash_t, string_t, identity<hash_t>> string_map {};
    std::vector<string> strings {};
    char16_t wstr_buf[0x8000] {};
    char8_t str_buf[0x10000] {};
    std::codecvt_utf8<char16_t> convert {};
    extern key_t key_bms[65536];
    extern key_t key_gms[65536];
    extern key_t key_kms[65536];
    key_t const (*const keys[3])[65536] {&key_bms, &key_gms, &key_kms};
    key_t const (*cur_key)[65536] {nullptr};
    std::deque<dir_t> directories {};
    std::deque<std::pair<img_t, int32_t>> imgs {};
    size_t file_start {};
    std::vector<string> resolve_path;


    string_t add_string(char8_t const * data, strsize_t size) {
        hash_t hash {2166136261UL};
        char8_t const * s {data};
        for (strsize_t i {size}; i; --i, ++s) {
            hash ^= static_cast<hash_t>(*s);
            hash *= 16777619UL;
        }
        string_t & id {string_map[hash]};
        if (id != 0) return id;
        id = static_cast<string_t>(strings.size());
        strings.push_back({static_cast<char8_t *>(alloc::small(size)), size});
        memcpy(strings.back().data, data, size);
        //For debugging purposes
        //std::cerr.write(data, size).put('\n');
        return id;
    }
    string_t read_enc_string() {
        strsize_t slen {};
        int8_t len {in::read<int8_t>()};
        if (len > 0) {
            slen = static_cast<strsize_t>(len == 127 ? in::read<int32_t>() : len);
            char16_t const * ows {reinterpret_cast<char16_t const *>(in::offset)};
            in::skip(slen * 2u);
            char16_t * ws {wstr_buf};
            uint16_t mask {0xAAAA};
            char16_t const * key {reinterpret_cast<char16_t const *>(*cur_key)};
            for (strsize_t i {slen}; i; --i, ++mask, ++ows, ++ws, ++key) {
                *ws = static_cast<char16_t>(*ows ^ *key ^ mask);
            }
            mbstate_t state {};
            char16_t const * fnext {};
            char8_t * tnext {};
            convert.out(state, wstr_buf, wstr_buf + slen, fnext, str_buf, str_buf + 0x10000, tnext);
            return add_string(str_buf, static_cast<strsize_t>(tnext - str_buf));
        }
        if (len < 0) {
            slen = static_cast<strsize_t>(len == -128 ? in::read<int32_t>() : -len);
            char8_t const * os {reinterpret_cast<char8_t const *>(in::offset)};
            in::skip(slen);
            char8_t * s {str_buf};
            uint8_t mask {0xAA};
            char8_t const * key {reinterpret_cast<char8_t const *>(*cur_key)};
            for (strsize_t i {slen}; i; --i, ++mask, ++os, ++s, ++key) {
                *s = static_cast<char8_t>(*os ^ *key ^ mask);
            }
            return add_string(str_buf, slen);
        }
        return 0;
    }
    string_t read_prop_string(size_t offset) {
        uint8_t a {in::read<uint8_t>()};
        switch (a) {
        case 0x00:
        case 0x73:
            return read_enc_string();
        case 0x01:
        case 0x1B:{
            size_t o {in::read<int32_t>() + offset};
            size_t p {in::tell()};
            in::seek(o);
            string_t s {read_enc_string()};
            in::seek(p);
            return s;
                  }
        default:
            throw std::runtime_error {"Unknown property string type: " + std::to_string(a)};
        }
    }
    void deduce_key() {
        int8_t len {in::read<int8_t>()};
        if (len >= 0) throw std::runtime_error {"I give up"};
        strsize_t slen {static_cast<strsize_t>(len == -128 ? in::read<int32_t>() : -len)};
        cur_key = nullptr;
        for (auto key : keys) {
            char8_t const * os {reinterpret_cast<char8_t const *>(in::offset)};
            uint8_t mask {0xAA};
            char8_t const * k {reinterpret_cast<char8_t const *>(*key)};
            bool valid {true};
            for (strsize_t i {slen}; i; --i && valid, ++mask, ++os, ++k) {
                char8_t c {static_cast<char8_t>(*os ^ *k ^ mask)};
                if (c < 0x20 || c >= 0x80) valid = false;
            }
            if (valid) cur_key = key;
        }
        if (!cur_key) throw std::runtime_error {"Failed to identify the locale"};
        in::skip(slen);
    }
    void sort_nodes(node_t first, node_t count) {
        std::sort(nodes.begin() + first, nodes.begin() + first + count, [](node const & n1, node const & n2) {
            if (&n1 == &n2) return false;
            string const & s1 {strings[n1.name]};
            string const & s2 {strings[n2.name]};
            int n {strncmp(s1.data, s2.data, std::min(s1.size, s2.size))};
            if (n < 0) return true;
            if (n > 0) return false;
            if (s1.size < s2.size) return true;
            if (s1.size > s2.size) return false;
            throw std::runtime_error {"Identical strings. This is baaaaaaaaaaaaaad"};
        });
    }
    void resolve_uols(node_t uol_node) {
        node & n {nodes[uol_node]};
        if (n.data_type == node::type::uol) {
            std::vector<string> path {resolve_path};
            string & s {strings[n.data.string]};
            strsize_t b {0};
            for (strsize_t i {0}; i < s.size; ++i) {
                if (s.data[i] == '/') {
                    if (i - b == 2 && strncmp(s.data + b, "..", 2) == 0) path.pop_back();
                    else path.push_back({s.data + b, static_cast<strsize_t>(i - b)});
                    b = ++i;
                }
            }
            path.push_back({s.data + b, static_cast<strsize_t>(s.size - b)});
            node_t search {0};
            for (string & s : path) {
                node & n {nodes[search]};
                bool found {false};
                for (node_t i {n.children}; i < n.children + n.num; ++i) {
                    node & nn {nodes[i]};
                    string & ss {strings[nn.name]};
                    if (s.size == ss.size && strncmp(s.data, ss.data, s.size) == 0) {
                        search = i;
                        found = true;
                        break;
                    }
                }
                if (!found) {//Damnit Nexon, get your shit together
                    n.data_type = node::type::none;
                    return;
                }
            }
            node & nn {nodes[search]};
            n.data_type = nn.data_type;
            n.children = nn.children;
            n.num = nn.num;
            n.data.integer = nn.data.integer;//Simply copies the whole value. It's a union anyway
            //Note, we do not copy the name
        } else {
            if (uol_node) resolve_path.push_back(strings[n.name]);//Ignore the root node
            for (node_t i {0}; i < n.num; ++i) resolve_uols(n.children + i);
            if (uol_node) resolve_path.pop_back();
        }
    }
    void directory(node_t dir_node) {
        node & n {nodes[dir_node]};
        node_t count {static_cast<node_t>(in::read_cint())};
        node_t ni {static_cast<node_t>(nodes.size())};
        n.num = static_cast<uint16_t>(count);
        n.children = ni;
        for (node_t i {0}; i < count; ++i) {
            nodes.emplace_back();
            node & nn {nodes.back()};
            uint8_t type {in::read<uint8_t>()};
            switch (type) {
            case 1:
                throw std::runtime_error {"Found the elusive type 1 directory"};
            case 2:{
                int32_t s {in::read<int32_t>()};
                size_t p {in::tell()};
                in::seek(file_start + s);
                type = in::read<uint8_t>();
                nn.name = read_enc_string();
                in::seek(p);
                break;
                   }
            case 3:
            case 4:
                nn.name = read_enc_string();
                break;
            default:
                throw std::runtime_error {"Unknown directory type"};
            }
            int32_t size {in::read_cint()};
            if (size <= 0) throw std::runtime_error {"Directory/img has invalid size!"};
            in::read_cint();//Checksum that nobody cares about
            in::skip(4);
            if (type == 3) directories.push_back(ni + i);
            else if (type == 4) imgs.emplace_back(ni + i, size);
            else throw std::runtime_error {"Unknown type 2 directory"};
        }
        nodes_to_sort.emplace_back(ni, count);
    }
    void sub_property(node_t, size_t);
    void extended_property(node_t prop_node, size_t offset) {
        node & n {nodes[prop_node]};
        string_t s {read_prop_string(offset)};
        string const & st {strings[s]};
        if (!strncmp(st.data, "Property", st.size)) {
            in::skip(2);
            sub_property(prop_node, offset);
        } else if (!strncmp(st.data, "Canvas", st.size)) {
            in::skip(1);
            if (in::read<uint8_t>() == 1) {
                in::skip(2);
                sub_property(prop_node, offset);
            }
            //Canvas stuff todo later
        } else if (!strncmp(st.data, "Shape2D#Vector2D", st.size)) {
            n.data_type = node::type::vector;
            n.data.vector[0] = in::read_cint();
            n.data.vector[1] = in::read_cint();
        } else if (!strncmp(st.data, "Shape2D#Convex2D", st.size)) {
            node_t count {static_cast<node_t>(in::read_cint())};
            node_t ni {static_cast<node_t>(nodes.size())};
            nodes.resize(nodes.size() + count);
            for (node_t i {0}; i < count; ++i) {
                node & nn {nodes[ni + i]};
                std::string es {std::to_string(i)};
                nn.name = add_string(es.c_str(), static_cast<strsize_t>(es.size()));
                extended_property(ni, offset);
            }
            nodes_to_sort.emplace_back(ni, count);
        } else if (!strncmp(st.data, "Sound_DX8", st.size)) {
            //Audio stuff
        } else if (!strncmp(st.data, "UOL", st.size)) {
            in::skip(1);
            n.data_type = node::type::uol;
            n.data.string = read_prop_string(offset);
        } else throw std::runtime_error {"Unknown sub property type: " + std::string {st.data, st.size}};
    }
    void sub_property(node_t prop_node, size_t offset) {
        node_t count {static_cast<node_t>(in::read_cint())};
        node & n {nodes[prop_node]};
        node_t ni {static_cast<node_t>(nodes.size())};
        n.num = static_cast<uint16_t>(count);
        n.children = ni;
        nodes.resize(nodes.size() + count);
        for (node_t i {0}; i < count; ++i) {
            node & nn {nodes[ni + i]};
            nn.name = read_prop_string(offset);
            uint8_t type {in::read<uint8_t>()};
            switch (type) {
            case 0x00://Turning null nodes into integers with an id. Useful for zmap.img
                nn.data_type = node::type::integer;
                nn.data.integer = i;
                break;
            case 0x0B:
            case 0x02:
                nn.data_type = node::type::integer;
                nn.data.integer = in::read<uint16_t>();
                break;
            case 0x03:
                nn.data_type = node::type::integer;
                nn.data.integer = in::read_cint();
                break;
            case 0x04:
                nn.data_type = node::type::real;
                nn.data.real = in::read<uint8_t>() == 0x80 ? in::read<float>() : 0.;
                break;
            case 0x05:
                nn.data_type = node::type::real;
                nn.data.real = in::read<double>();
                break;
            case 0x08:
                nn.data_type = node::type::string;
                nn.data.string = read_prop_string(offset);
                break;
            case 0x09:{
                size_t p {in::read<int32_t>() + in::tell()};
                extended_property(ni + i, offset);
                in::seek(p);
                break;
                      }
            default:
                throw std::runtime_error {"Unknown sub property type: " + std::to_string(type)};
            }
        }
        nodes_to_sort.emplace_back(ni, count);
    }
    void img(node_t img_node, int32_t size) {
        size_t p {in::tell()};
        in::skip(1);
        deduce_key();
        in::skip(2);
        sub_property(img_node, p);
        in::seek(p + size);
    }
    void wztonx(std::string filename) {
        in::open(filename);
        filename.erase(filename.find_last_of('.')).append(".nx");
        uint32_t magic {in::read<uint32_t>()};
        if (magic != 0x31474B50) throw std::runtime_error {"Not a valid WZ file"};
        in::skip(8);
        file_start = in::read<uint32_t>();
        in::seek(file_start + 2);
        in::read_cint();
        in::skip(1);
        deduce_key();
        in::seek(file_start + 2);
        add_string("", 0);
        std::cout << "Opened file" << std::endl;
        directories.push_back(0);
        while (!directories.empty()) {
            directory(directories.front());
            directories.pop_front();
        }
        std::cout << "Parsed directories" << std::endl;
        while (!imgs.empty()) {
            img(imgs.front().first, imgs.front().second);
            imgs.pop_front();
        }
        std::cout << "Parsed images" << std::endl;
        resolve_uols(0);
        for (auto const & n : nodes_to_sort) sort_nodes(n.first, n.second);
        std::cout << "Node cleanup finished" << std::endl;
        size_t node_offset = 52;
        node_offset += 0x10 - (node_offset & 0xf);
        size_t string_table_offset = node_offset + nodes.size() * 20;
        string_table_offset += 0x10 - (string_table_offset & 0xf);
        size_t string_offset = string_table_offset + strings.size() * 8;
        string_offset += 0x10 - (string_offset & 0xf);
        size_t bitmap_table_offset = string_offset + strings.size() * 2;
        for (auto const & s : strings) bitmap_table_offset += s.size;
        bitmap_table_offset += 0x10 - (bitmap_table_offset & 0xf);
        out::open(filename, bitmap_table_offset);
        out::seek(0);
        out::write<uint32_t>(0x34474B50);
        out::write<uint32_t>(static_cast<uint32_t>(nodes.size()));
        out::write<uint64_t>(node_offset);
        out::write<uint32_t>(static_cast<uint32_t>(strings.size()));
        out::write<uint64_t>(string_table_offset);
        //No bitmap or audio support yet
        out::write<uint32_t>(0);
        out::write<uint64_t>(0);
        out::write<uint32_t>(0);
        out::write<uint64_t>(0);
        std::cout << "Opened output" << std::endl;
        out::seek(node_offset);
        out::write(nodes.data(), nodes.size() * 20);
        std::cout << "Wrote nodes" << std::endl;
        out::seek(string_table_offset);
        size_t next_str {string_offset};
        for (auto const & s : strings) {
            out::write<uint64_t>(next_str);
            next_str += s.size + 2;
        }
        out::seek(string_offset);
        for (auto const & s : strings) {
            out::write<uint16_t>(s.size);
            out::write(s.data, s.size);
        }
        std::cout << "Wrote strings" << std::endl;
        out::close();
        in::close();
        std::cout << "Done" << std::endl;
    }
}
int main(int argc, char ** argv) {
    clock_t t1 {clock()};
    if (argc > 1) nl::wztonx(argv[1]);
    else nl::wztonx("Data.wz");
    clock_t t2 {clock()};
    std::cout << "Took " << t2 - t1 << " ms" << std::endl;
}
