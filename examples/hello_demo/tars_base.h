// **********************************************************************
// This file was generated by a TARS parser!
// TARS version 2.4.13.
// **********************************************************************

#ifndef __TARS_BASE_H_
#define __TARS_BASE_H_

#include <map>
#include <string>
#include <vector>
#include "tup/Tars.h"
#include "tup/TarsJson.h"
using namespace std;


namespace Base
{
    struct BaseInfo : public tars::TarsStructBase
    {
    public:
        static string className()
        {
            return "Base.BaseInfo";
        }
        static string MD5()
        {
            return "75f1be2757c92eb6699b52ef20b9f989";
        }
        BaseInfo()
        {
            resetDefautlt();
        }
        void resetDefautlt()
        {
            name = "";
            id = 0;
        }
        template<typename WriterT>
        void writeTo(tars::TarsOutputStream<WriterT>& _os) const
        {
            if (name != "")
            {
                _os.write(name, 1);
            }
            if (id != 0)
            {
                _os.write(id, 2);
            }
        }
        template<typename ReaderT>
        void readFrom(tars::TarsInputStream<ReaderT>& _is)
        {
            resetDefautlt();
            _is.read(name, 1, false);
            _is.read(id, 2, false);
        }
        tars::JsonValueObjPtr writeToJson() const
        {
            tars::JsonValueObjPtr p = new tars::JsonValueObj();
            p->value["name"] = tars::JsonOutput::writeJson(name);
            p->value["id"] = tars::JsonOutput::writeJson(id);
            return p;
        }
        string writeToJsonString() const
        {
            return tars::TC_Json::writeValue(writeToJson());
        }
        void readFromJson(const tars::JsonValuePtr & p, bool isRequire = true)
        {
            resetDefautlt();
            if(NULL == p.get() || p->getType() != tars::eJsonTypeObj)
            {
                char s[128];
                snprintf(s, sizeof(s), "read 'struct' type mismatch, get type: %d.", (p.get() ? p->getType() : 0));
                throw tars::TC_Json_Exception(s);
            }
            tars::JsonValueObjPtr pObj=tars::JsonValueObjPtr::dynamicCast(p);
            tars::JsonInput::readJson(name,pObj->value["name"], false);
            tars::JsonInput::readJson(id,pObj->value["id"], false);
        }
        void readFromJsonString(const string & str)
        {
            readFromJson(tars::TC_Json::getValue(str));
        }
        ostream& display(ostream& _os, int _level=0) const
        {
            tars::TarsDisplayer _ds(_os, _level);
            _ds.display(name,"name");
            _ds.display(id,"id");
            return _os;
        }
        ostream& displaySimple(ostream& _os, int _level=0) const
        {
            tars::TarsDisplayer _ds(_os, _level);
            _ds.displaySimple(name, true);
            _ds.displaySimple(id, false);
            return _os;
        }
    public:
        std::string name;
        tars::Int32 id;
    };
    inline bool operator==(const BaseInfo&l, const BaseInfo&r)
    {
        return l.name == r.name && l.id == r.id;
    }
    inline bool operator!=(const BaseInfo&l, const BaseInfo&r)
    {
        return !(l == r);
    }
    inline ostream& operator<<(ostream & os,const BaseInfo&r)
    {
        os << r.writeToJsonString();
        return os;
    }
    inline istream& operator>>(istream& is,BaseInfo&l)
    {
        std::istreambuf_iterator<char> eos;
        std::string s(std::istreambuf_iterator<char>(is), eos);
        l.readFromJsonString(s);
        return is;
    }


}



#endif
