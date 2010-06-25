#include "SimpleSerializer.h"
#include "ArchiveBinary.h"
#include "ArchiveXML.h"

#include "Foundation/Memory/Endian.h"

using namespace Reflect;

template <class T>
SimpleSerializer<T>::SimpleSerializer()
{

}

template <class T>
SimpleSerializer<T>::~SimpleSerializer()
{

}

template <class T>
bool SimpleSerializer<T>::IsCompact() const
{ 
    return true; 
}

template <class T>
void SimpleSerializer<T>::ConnectData(Nocturnal::HybridPtr<void> data)
{
    __super::ConnectData( data );

    m_Data.Connect( Nocturnal::HybridPtr<DataType> (data.Address(), data.State()) );
}

template <class T>
bool SimpleSerializer<T>::Set(const Serializer* src, u32 flags)
{
    const SimpleSerializerT* rhs = ConstObjectCast<SimpleSerializerT>(src);
    if (!rhs)
    {
        return false;
    }

    m_Data.Set( rhs->m_Data.Get() );

    return true;
}

template <class T>
bool SimpleSerializer<T>::Equals(const Serializer* s) const
{
    const SimpleSerializerT* rhs = ConstObjectCast<SimpleSerializerT>(s);
    if (!rhs)
    {
        return false;
    }

    return rhs->m_Data.Get() == m_Data.Get();
}

template <class T>
void SimpleSerializer<T>::Serialize(Archive& archive) const
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            archive.GetStream() << m_Data.Get();
#endif
            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));

            binary.GetStream().Write(m_Data.Ptr()); 
            break;
        }
    }
}

template <class T>
void SimpleSerializer<T>::Serialize(const Nocturnal::BasicBufferPtr& buffer, const tchar* debugStr) const
{
    T val = m_Data.Get();

    Nocturnal::Swizzle( val, buffer->GetPlatform() != Nocturnal::BufferPlatforms::x86 );

    buffer->AddBuffer( (const u8*)&val, sizeof(T), debugStr );
}

template <class T>
std::ostream& SimpleSerializer<T>::operator >> (std::ostream& stream) const
{
    if (!TranslateOutput( stream ))
    {
        stream << m_Data.Get();
    }
    return stream;
}

template <class T>
void SimpleSerializer<T>::Deserialize(Archive& archive)
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            archive.GetStream() >> m_Data.Ref();
#endif
            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));

            binary.GetStream().Read(m_Data.Ptr()); 
            break;
        }
    }
}

template <class T>
std::istream& SimpleSerializer<T>::operator << (std::istream& stream)
{
    if (!TranslateInput( stream ))
    {
        stream >> m_Data.Ref();

        if ( m_Instance && m_Field && m_Field->m_Type->GetReflectionType() == ReflectionTypes::Class )
        {
            Element* element = (Element*)m_Instance;
            element->RaiseChanged( m_Field );
        }
    }
    return stream;
}

//
// Specializations
//

// must escape strings to account for special "evil" characters... like ", &, `, etc...
template <>
void StringSerializer::Serialize(Archive& archive) const
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            archive.GetStream() << "<![CDATA[" << m_Data.Get() << "]]>";
#endif
            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));

            i32 index = binary.GetStrings().Insert(m_Data.Get());
            binary.GetStream().Write(&index); 
            break;
        }
    }
}

// keep reading the string until we run out of buffer
template <>
void StringSerializer::Deserialize(Archive& archive)
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            std::streamsize size = archive.GetStream().BytesAvailable(); 
            m_Data->resize( (size_t) size);
            archive.GetStream().ReadBuffer(const_cast<char*>(m_Data->c_str()), size);
#endif
            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));

            i32 index;
            binary.GetStream().Read(&index); 
            m_Data.Set( binary.GetStrings().GetString(index) );
            break;
        }
    }
}

template<>
std::ostream& StringSerializer::operator >> (std::ostream& stream) const
{
    if (!TranslateOutput( stream ))
    {
        stream << m_Data.Get();
    }
    return stream;
}

template<>
std::istream& StringSerializer::operator << (std::istream& stream)
{
    if (!TranslateInput( stream ))
    {
        std::streamsize size = stream.rdbuf()->in_avail();
        m_Data->resize( (size_t) size);
        stream.read(const_cast<char*>(m_Data.Get().c_str()), size);
    }
    return stream;
}

// this is a char, we must treat it as a number
template <>
void U8Serializer::Serialize(Archive& archive) const
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            u16 tmp = m_Data.Get();
            archive.GetStream() << tmp;
#endif
            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));

            binary.GetStream().Write(m_Data.Ptr()); 
            break;
        }
    }
}

template <>
void U8Serializer::Deserialize(Archive& archive)
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            u16 tmp;
            archive.GetStream() >> tmp;
            m_Data.Set( (unsigned char)tmp );
#endif

            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));
            
            binary.GetStream().Read(m_Data.Ptr()); 
            break;
        }
    }
}

template<>
std::ostream& U8Serializer::operator >> (std::ostream& stream) const
{
    if (!TranslateOutput( stream ))
    {
        u16 val = m_Data.Get();
        stream << val;
    }
    return stream;
}

template<>
std::istream& U8Serializer::operator << (std::istream& stream)
{
    if (!TranslateInput( stream ))
    {
        u16 val;
        stream >> val;
        m_Data.Set( (u8)val );
    }
    return stream;
}

// this is a char, we must treat it as a number
template <>
void I8Serializer::Serialize(Archive& archive) const
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            i16 tmp = m_Data.Get();
            archive.GetStream() << tmp;
#endif
            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));

            binary.GetStream().Write(m_Data.Ptr()); 
            break;
        }
    }
}

template <>
void I8Serializer::Deserialize(Archive& archive)
{
    switch (archive.GetType())
    {
    case ArchiveTypes::XML:
        {
#ifdef REFLECT_XML_SUPPORT
            i16 tmp;
            archive.GetStream() >> tmp;
            m_Data.Set( (char)tmp );
#endif

            break;
        }

    case ArchiveTypes::Binary:
        {
            ArchiveBinary& binary (static_cast<ArchiveBinary&>(archive));

            binary.GetStream().Read(m_Data.Ptr()); 
            break;
        }
    }
}

template<>
std::ostream& I8Serializer::operator >> (std::ostream& stream) const
{
    if (!TranslateOutput( stream ))
    {
        i16 val = m_Data.Get();
        stream << val;
    }
    return stream;
}

template<>
std::istream& I8Serializer::operator << (std::istream& stream)
{
    if (!TranslateInput( stream ))
    {
        i16 val;
        stream >> val;
        m_Data.Set( (u8)val );
    }
    return stream;
}

template SimpleSerializer<bool>;
template SimpleSerializer<u8>;
template SimpleSerializer<i8>;
template SimpleSerializer<u16>;
template SimpleSerializer<i16>;
template SimpleSerializer<u32>;
template SimpleSerializer<i32>;
template SimpleSerializer<u64>;
template SimpleSerializer<i64>;
template SimpleSerializer<f32>;
template SimpleSerializer<f64>;
template SimpleSerializer<std::string>;
template SimpleSerializer<Nocturnal::GUID>;
template SimpleSerializer<Nocturnal::TUID>;

template SimpleSerializer<Math::Vector2>;
template SimpleSerializer<Math::Vector3>;
template SimpleSerializer<Math::Vector4>;
template SimpleSerializer<Math::Quaternion>;

template SimpleSerializer<Math::Matrix3>;
template SimpleSerializer<Math::Matrix4>;

template SimpleSerializer<Math::Color3>;
template SimpleSerializer<Math::HDRColor3>;
template SimpleSerializer<Math::Color4>;
template SimpleSerializer<Math::HDRColor4>;

REFLECT_DEFINE_CLASS(StringSerializer);
REFLECT_DEFINE_CLASS(BoolSerializer);
REFLECT_DEFINE_CLASS(U8Serializer);
REFLECT_DEFINE_CLASS(I8Serializer);
REFLECT_DEFINE_CLASS(U16Serializer);
REFLECT_DEFINE_CLASS(I16Serializer);
REFLECT_DEFINE_CLASS(U32Serializer);
REFLECT_DEFINE_CLASS(I32Serializer);
REFLECT_DEFINE_CLASS(U64Serializer);
REFLECT_DEFINE_CLASS(I64Serializer);
REFLECT_DEFINE_CLASS(F32Serializer);
REFLECT_DEFINE_CLASS(F64Serializer);
REFLECT_DEFINE_CLASS(GUIDSerializer);
REFLECT_DEFINE_CLASS(TUIDSerializer);

REFLECT_DEFINE_CLASS(Vector2Serializer);
REFLECT_DEFINE_CLASS(Vector3Serializer);
REFLECT_DEFINE_CLASS(Vector4Serializer);
REFLECT_DEFINE_CLASS(Matrix3Serializer);
REFLECT_DEFINE_CLASS(Matrix4Serializer);
REFLECT_DEFINE_CLASS(QuaternionSerializer);

REFLECT_DEFINE_CLASS(Color3Serializer);
REFLECT_DEFINE_CLASS(Color4Serializer);
REFLECT_DEFINE_CLASS(HDRColor3Serializer);
REFLECT_DEFINE_CLASS(HDRColor4Serializer);