#pragma once

#include <sstream>
#include <iostream>

#include "Foundation/Automation/Event.h"
#include "Foundation/Memory/HybridPtr.h"
#include "Foundation/SmartBuffer/BasicBuffer.h"
#include "Foundation/Reflect/Object.h"
#include "Foundation/Reflect/Archive.h"

namespace Helium
{
    namespace Reflect
    {
        namespace DataFlags
        {
            enum DataFlag
            {
                Shallow   = 1 << 0,
            };
        }

        typedef DataFlags::DataFlag DataFlag;

        //
        // A Data is an Object that knows how to read/write data
        //  from any kind of support Archive type (XML and Binary), given
        //  an address in memory to serialize/deserialize data to/from
        //

        class FOUNDATION_API Data : public Object
        {
        protected:
            // derived classes will use this
            template<class T>
            class Pointer
            {
            public:
                Pointer()
                    : m_Owned( false )
                {

                }

                ~Pointer()
                {
                    Deallocate();
                }

                template<class DataT>
                void Allocate()
                {
                    m_Target = new DataT;
                    m_Owned = true;
                }

                void Deallocate()
                {
                    if ( m_Owned && m_Target )
                    {
                        delete m_Target;
                        m_Target = static_cast< const T* >( NULL );
                    }
                }

                void Connect(Helium::HybridPtr<T> pointer)
                {
                    Deallocate();
                    m_Target = pointer;
                    m_Owned = false;
                }

                void Disconnect()
                {
                    Deallocate();
                }

                const T* operator->() const
                {
                    return m_Target;
                }

                T* operator->()
                {
                    return m_Target;
                }

                operator const T*() const
                {
                    return m_Target;
                }

                operator T*()
                {
                    return m_Target;
                }

            private:
                Helium::HybridPtr<T>    m_Target;
                bool                    m_Owned;
            };

            // the instance we are processing, if any
            Helium::HybridPtr<void> m_Instance;

            // the field we are processing, if any
            const Field* m_Field;

        public:
            REFLECT_DECLARE_ABSTRACT( Data, Object );

            // instance init and cleanup
            Data();
            virtual ~Data();

            // static init and cleanup
            static void Initialize();
            static void Cleanup();

            //
            // Connection
            //

            // set the address to interface with
            virtual void ConnectData(Helium::HybridPtr<void> data) = 0;

            // connect to a field of an object
            void ConnectField(Helium::HybridPtr<void> instance, const Field* field, uintptr_t offsetInField = 0);

            // reset all pointers
            void Disconnect();

            //
            // Specializations
            //

            template<class T>
            static inline T* GetData(Data*)
            {
                return NULL;
            }

            template<class T>
            static inline const T* GetData(const Data*)
            {
                return NULL;
            }

            //
            // Creation templates
            //

            template <class T>
            static DataPtr Create()
            {
                const Class* dataClass = Reflect::GetDataClass<T>();
                HELIUM_ASSERT( dataClass );

                return AssertCast<Data>( Registry::GetInstance()->CreateInstance( dataClass ) );
            }

            template <class T>
            static DataPtr Create(const T& value)
            {
                DataPtr ser = Create<T>();

                if (ser.ReferencesObject())
                {
                    SetValue(ser, value);
                }

                return ser;
            }

            template <class T>
            static DataPtr Bind(T& value, void* instance, const Field* field)
            {
                DataPtr ser = Create<T>();

                if (ser.ReferencesObject())
                {
                    ser->ConnectData( &value );
                    ser->m_Instance = instance;
                    ser->m_Field = field;
                }

                return ser;
            }

            template <class T>
            static DataPtr Bind(const T& value, const void* instance, const Field* field)
            {
                DataPtr ser = Create<T>();

                if (ser.ReferencesObject())
                {
                    ser->ConnectData( &value );
                    ser->m_Instance = instance;
                    ser->m_Field = field;
                }

                return ser;
            }

            //
            // Value templates
            //

            template <typename T>
            static bool GetValue(const Data* ser, T& value);

            template <typename T>
            static bool SetValue(Data* ser, const T& value, bool raiseEvents = true);


            //
            // Data Management
            //

            // check to see if a cast is supported
            static bool CastSupported(const Class* srcType, const Class* destType);

            // convert value data from one data to another
            static bool CastValue(const Data* src, Data* dest, uint32_t flags = 0);

            // copies value data from one data to another
            virtual bool Set(const Data* src, uint32_t flags = 0) = 0;

            // assign
            Data& operator=(const Data* rhs)
            {
                Set(rhs);
                return *this;
            }
            Data& operator=(const Data& rhs)
            {
                Set(&rhs);
                return *this;
            }

            // equality
            bool operator==(const Data* rhs) const
            {
                return Equals(rhs);
            }
            bool operator== (const Data& rhs) const
            {
                return Equals(&rhs);
            }

            // inequality
            bool operator!= (const Data* rhs) const
            {
                return !Equals(rhs);
            }
            bool operator!= (const Data& rhs) const
            {
                return !Equals(&rhs);
            }

            //
            // Serialization
            //

            // is this data worth serializing? (perhaps its an empty container?)
            virtual bool ShouldSerialize();

            // data serialization (extract to smart buffer)
            virtual void Serialize(const Helium::BasicBufferPtr& buffer, const tchar_t* debugStr) const;

            // data serialization (extract to archive)
            virtual void Serialize(Archive& archive) const = 0;

            // data deserialization (insert from archive)
            virtual void Deserialize(Archive& archive) = 0;

            // text serialization (extract to text stream)
            virtual tostream& operator>> (tostream& stream) const;

            // text deserialization (insert from text stream)
            virtual tistream& operator<< (tistream& stream);

            //
            // Visitor
            //

            virtual void Accept(Visitor& visitor) HELIUM_OVERRIDE;
        };

        //
        // These are the global entry points for our virtual insert an extract functions
        //

        inline tostream& operator<< (tostream& stream, const Data& s)
        {
            return s >> stream;
        }

        inline tistream& operator>> (tistream& stream, Data& s)
        {
            return s << stream;
        }

        //
        // GetValue / SetValue templates
        //

        template <class T>
        inline bool Data::GetValue(const Data* ser, T& value)
        {
            if ( ser == NULL )
            {
                return false;
            }

            bool result = false;
            const Class* dataClass = Reflect::GetDataClass<T>();

            // if you die here, then you are not using serializers that
            //  fully implement the type deduction functions above
            HELIUM_ASSERT( dataClass != NULL );

            // sanity check our object type
            if ( ser->IsClass( dataClass ) )
            {
                // get internal data pointer
                const T* data = GetData<T>( ser );
                HELIUM_ASSERT( data != NULL );

                // make the copy
                value = *data;
                result = true;
            }
            else
            {
                // create a temporary data of the value type
                DataPtr temp = AssertCast<Data>( Registry::GetInstance()->CreateInstance( dataClass ) );

                // connect the temp data to the temp value
                T tempValue; temp->ConnectData( &tempValue );

                // cast into the temp value
                if (Data::CastValue( ser, temp ))
                {
                    // make the copy
                    value = tempValue;
                    result = true;
                }
            }

            HELIUM_ASSERT(result);
            return result;
        }

        template <class T>
        inline bool Data::SetValue(Data* ser, const T& value, bool raiseEvents)
        {
            if ( ser == NULL )
            {
                return false;
            }

            bool result = false;
            const Class* dataClass = Reflect::GetDataClass< T >();

            // if you die here, then you are not using serializers that
            //  fully implement the type deduction functions above
            HELIUM_ASSERT( dataClass != NULL );

            // sanity check our object type
            if ( ser->IsClass( dataClass ) )
            {
                // get internal data pointer
                T* data = GetData<T>( ser );
                HELIUM_ASSERT( data != NULL );

                // if you die here, then you are probably in release and this should crash
                (*data) = value;
                result = true;
            }
            else
            {
                // create a temporary data of the value type
                DataPtr temp = AssertCast<Data>( Registry::GetInstance()->CreateInstance( dataClass ) );

                // connect the temp data to the temp value
                temp->ConnectData( &value );

                // cast into the data
                if (Data::CastValue( ser, temp ))
                {
                    result = true;
                }
            }

            if (result)
            {
                // Notify interested listeners that the data has changed.
                if ( raiseEvents && ser && ser->m_Instance && ser->m_Field && ser->m_Field->m_Composite->GetReflectionType() == ReflectionTypes::Class )
                {
                    Object* object = (Object*)ser->m_Instance.Mutable();
                    object->RaiseChanged( ser->m_Field );
                }
            }

            HELIUM_ASSERT(result);
            return result;
        }

        template <class T>
        static const Class* GetDataClass()
        {
            return NULL;
        }
    }
}
