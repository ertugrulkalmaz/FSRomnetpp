
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include "FsrPkt_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace inet {
namespace inetmanet {

Register_Class(FsrPkt)

FsrPkt::FsrPkt() : ::inet::FieldsChunk()
{
}

FsrPkt::FsrPkt(const FsrPkt& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

FsrPkt::~FsrPkt()
{
    delete [] this->msg_var;
}

FsrPkt& FsrPkt::operator=(const FsrPkt& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void FsrPkt::copy(const FsrPkt& other)
{
    this->reduceFuncionality_var = other.reduceFuncionality_var;
    this->pkt_seq_num_var = other.pkt_seq_num_var;
    this->sn_var = other.sn_var;
    this->send_time_var = other.send_time_var;
    delete [] this->msg_var;
    this->msg_var = (other.msg_arraysize==0) ? nullptr : new FsrMsg[other.msg_arraysize];
    msg_arraysize = other.msg_arraysize;
    for (size_t i = 0; i < msg_arraysize; i++) {
        this->msg_var[i] = other.msg_var[i];
    }
}

void FsrPkt::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->reduceFuncionality_var);
    doParsimPacking(b,this->pkt_seq_num_var);
    doParsimPacking(b,this->sn_var);
    doParsimPacking(b,this->send_time_var);
    b->pack(msg_arraysize);
    doParsimArrayPacking(b,this->msg_var,msg_arraysize);
}

void FsrPkt::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->reduceFuncionality_var);
    doParsimUnpacking(b,this->pkt_seq_num_var);
    doParsimUnpacking(b,this->sn_var);
    doParsimUnpacking(b,this->send_time_var);
    delete [] this->msg_var;
    b->unpack(msg_arraysize);
    if (msg_arraysize == 0) {
        this->msg_var = nullptr;
    } else {
        this->msg_var = new FsrMsg[msg_arraysize];
        doParsimArrayUnpacking(b,this->msg_var,msg_arraysize);
    }
}

bool FsrPkt::reduceFuncionality() const
{
    return this->reduceFuncionality_var;
}

void FsrPkt::setReduceFuncionality(bool reduceFuncionality)
{
    handleChange();
    this->reduceFuncionality_var = reduceFuncionality;
}

short FsrPkt::pkt_seq_num() const
{
    return this->pkt_seq_num_var;
}

void FsrPkt::setPkt_seq_num(short pkt_seq_num)
{
    handleChange();
    this->pkt_seq_num_var = pkt_seq_num;
}

long FsrPkt::sn() const
{
    return this->sn_var;
}

void FsrPkt::setSn(long sn)
{
    handleChange();
    this->sn_var = sn;
}

double FsrPkt::send_time() const
{
    return this->send_time_var;
}

void FsrPkt::setSend_time(double send_time)
{
    handleChange();
    this->send_time_var = send_time;
}

size_t FsrPkt::msgArraySize() const
{
    return msg_arraysize;
}

const FsrMsg& FsrPkt::msg(size_t k) const
{
    if (k >= msg_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)msg_arraysize, (unsigned long)k);
    return this->msg_var[k];
}

void FsrPkt::setMsgArraySize(size_t newSize)
{
    handleChange();
    FsrMsg *msg_var2 = (newSize==0) ? nullptr : new FsrMsg[newSize];
    size_t minSize = msg_arraysize < newSize ? msg_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        msg_var2[i] = this->msg_var[i];
    delete [] this->msg_var;
    this->msg_var = msg_var2;
    msg_arraysize = newSize;
}

void FsrPkt::setMsg(size_t k, const FsrMsg& msg)
{
    if (k >= msg_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)msg_arraysize, (unsigned long)k);
    handleChange();
    this->msg_var[k] = msg;
}

void FsrPkt::insertMsg(size_t k, const FsrMsg& msg)
{
    if (k > msg_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)msg_arraysize, (unsigned long)k);
    handleChange();
    size_t newSize = msg_arraysize + 1;
    FsrMsg *msg_var2 = new FsrMsg[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        msg_var2[i] = this->msg_var[i];
    msg_var2[k] = msg;
    for (i = k + 1; i < newSize; i++)
        msg_var2[i] = this->msg_var[i-1];
    delete [] this->msg_var;
    this->msg_var = msg_var2;
    msg_arraysize = newSize;
}

void FsrPkt::appendMsg(const FsrMsg& msg)
{
    insertMsg(msg_arraysize, msg);
}

void FsrPkt::eraseMsg(size_t k)
{
    if (k >= msg_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)msg_arraysize, (unsigned long)k);
    handleChange();
    size_t newSize = msg_arraysize - 1;
    FsrMsg *msg_var2 = (newSize == 0) ? nullptr : new FsrMsg[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        msg_var2[i] = this->msg_var[i];
    for (i = k; i < newSize; i++)
        msg_var2[i] = this->msg_var[i+1];
    delete [] this->msg_var;
    this->msg_var = msg_var2;
    msg_arraysize = newSize;
}

class FsrPktDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_reduceFuncionality,
        FIELD_pkt_seq_num,
        FIELD_sn,
        FIELD_send_time,
        FIELD_msg,
    };
  public:
    FsrPktDescriptor();
    virtual ~FsrPktDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(FsrPktDescriptor)

FsrPktDescriptor::FsrPktDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::inetmanet::FsrPkt)), "inet::FieldsChunk")
{
    propertyNames = nullptr;
}

FsrPktDescriptor::~FsrPktDescriptor()
{
    delete[] propertyNames;
}

bool FsrPktDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<FsrPkt *>(obj)!=nullptr;
}

const char **FsrPktDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = { "omitGetVerb", "fieldNameSuffix",  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *FsrPktDescriptor::getProperty(const char *propertyName) const
{
    if (!strcmp(propertyName, "omitGetVerb")) return "true";
    if (!strcmp(propertyName, "fieldNameSuffix")) return "_var";
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int FsrPktDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 5+base->getFieldCount() : 5;
}

unsigned int FsrPktDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_reduceFuncionality
        FD_ISEDITABLE,    // FIELD_pkt_seq_num
        FD_ISEDITABLE,    // FIELD_sn
        FD_ISEDITABLE,    // FIELD_send_time
        FD_ISARRAY | FD_ISRESIZABLE,    // FIELD_msg
    };
    return (field >= 0 && field < 5) ? fieldTypeFlags[field] : 0;
}

const char *FsrPktDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "reduceFuncionality",
        "pkt_seq_num",
        "sn",
        "send_time",
        "msg",
    };
    return (field >= 0 && field < 5) ? fieldNames[field] : nullptr;
}

int FsrPktDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "reduceFuncionality") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "pkt_seq_num") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "sn") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "send_time") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "msg") == 0) return baseIndex + 4;
    return base ? base->findField(fieldName) : -1;
}

const char *FsrPktDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "bool",    // FIELD_reduceFuncionality
        "short",    // FIELD_pkt_seq_num
        "long",    // FIELD_sn
        "double",    // FIELD_send_time
        "inet::inetmanet::FsrMsg",    // FIELD_msg
    };
    return (field >= 0 && field < 5) ? fieldTypeStrings[field] : nullptr;
}

const char **FsrPktDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *FsrPktDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int FsrPktDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        case FIELD_msg: return pp->msgArraySize();
        default: return 0;
    }
}

void FsrPktDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        case FIELD_msg: pp->setMsgArraySize(size); break;
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'FsrPkt'", field);
    }
}

const char *FsrPktDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string FsrPktDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        case FIELD_reduceFuncionality: return bool2string(pp->reduceFuncionality());
        case FIELD_pkt_seq_num: return long2string(pp->pkt_seq_num());
        case FIELD_sn: return long2string(pp->sn());
        case FIELD_send_time: return double2string(pp->send_time());
        case FIELD_msg: return pp->msg(i).str();
        default: return "";
    }
}

void FsrPktDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        case FIELD_reduceFuncionality: pp->setReduceFuncionality(string2bool(value)); break;
        case FIELD_pkt_seq_num: pp->setPkt_seq_num(string2long(value)); break;
        case FIELD_sn: pp->setSn(string2long(value)); break;
        case FIELD_send_time: pp->setSend_time(string2double(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FsrPkt'", field);
    }
}

omnetpp::cValue FsrPktDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        case FIELD_reduceFuncionality: return pp->reduceFuncionality();
        case FIELD_pkt_seq_num: return pp->pkt_seq_num();
        case FIELD_sn: return (omnetpp::intval_t)(pp->sn());
        case FIELD_send_time: return pp->send_time();
        case FIELD_msg: return omnetpp::toAnyPtr(&pp->msg(i)); break;
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'FsrPkt' as cValue -- field index out of range?", field);
    }
}

void FsrPktDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        case FIELD_reduceFuncionality: pp->setReduceFuncionality(value.boolValue()); break;
        case FIELD_pkt_seq_num: pp->setPkt_seq_num(omnetpp::checked_int_cast<short>(value.intValue())); break;
        case FIELD_sn: pp->setSn(omnetpp::checked_int_cast<long>(value.intValue())); break;
        case FIELD_send_time: pp->setSend_time(value.doubleValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FsrPkt'", field);
    }
}

const char *FsrPktDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr FsrPktDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        case FIELD_msg: return omnetpp::toAnyPtr(&pp->msg(i)); break;
        default: return omnetpp::any_ptr(nullptr);
    }
}

void FsrPktDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    FsrPkt *pp = omnetpp::fromAnyPtr<FsrPkt>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FsrPkt'", field);
    }
}

}  // namespace inetmanet
}  // namespace inet

namespace omnetpp {

}  // namespace omnetpp

