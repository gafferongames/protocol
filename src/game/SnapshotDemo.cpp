#include "SnapshotDemo.h"

#ifdef CLIENT

#include "Cubes.h"
#include "Global.h"
#include "vectorial/vec3f.h"
#include "vectorial/quat4f.h"
#include "protocol/Stream.h"
#include "protocol/SlidingWindow.h"
#include "protocol/PacketFactory.h"
#include "network/Simulator.h"

static const int MaxPacketSize = 32 * 1024;         // this has to be really large for the worst case!

static const int NumCubes = 900 + MaxPlayers;

//static const int LeftPort = 1000;
static const int RightPort = 1001;

static const float Latency = 1.0;
static const float PacketLoss = 1.0f;
static const float Jitter = 1.0f / 60.0f;

enum SnapshotMode
{
    SNAPSHOT_MODE_NAIVE_60PPS,                  // 1. naive snapshots with uncompressed data no jitter @ 60pps
    SNAPSHOT_MODE_NAIVE_60PPS_JITTER,           // 2. naive snapshots with jitter +/2 frames @ 60pps
    SNAPSHOT_MODE_NAIVE_10PPS_POPPING,          // 3. naive snapshots at 10pps (popping)
    SNAPSHOT_MODE_NAIVE_10PPS_INTERPOLATION,    // 4. naive snapshots at 10pps with interpolation
    // ...
    SNAPSHOT_NUM_MODES
};

const SnapshotMode SnapshotDefaultMode = SNAPSHOT_MODE_NAIVE_60PPS;      // update this as working on each mode

enum SnapshotPackets
{
    SNAPSHOT_NAIVE_PACKET,
    SNAPSHOT_ACK_PACKET,
    SNAPSHOT_NUM_PACKETS
};

struct CubeState
{
    bool interacting;
    vectorial::vec3f position;
    vectorial::quat4f orientation;
};

template <typename Stream> void serialize_vector( Stream & stream, vectorial::vec3f & vector )
{
    float values[3];
    if ( Stream::IsWriting )
    {
        values[0] = vector.x();
        values[1] = vector.y();
        values[2] = vector.z();
    }
    serialize_float( stream, values[0] );
    serialize_float( stream, values[1] );
    serialize_float( stream, values[2] );
    if ( Stream::IsReading )
        vector.load( values );
}

template <typename Stream> void serialize_quaternion( Stream & stream, vectorial::quat4f & quaternion )
{
    serialize_float( stream, quaternion.x );
    serialize_float( stream, quaternion.y );
    serialize_float( stream, quaternion.z );
    serialize_float( stream, quaternion.w );
}

struct SnapshotNaivePacket : public protocol::Packet
{
    uint16_t sequence;

    SnapshotNaivePacket() : Packet( SNAPSHOT_NAIVE_PACKET )
    {
        sequence = 0;
    }

    PROTOCOL_SERIALIZE_OBJECT( stream )
    {
        serialize_uint16( stream, sequence );
        for ( int i = 0; i < NumCubes; ++i )
        {
            serialize_bool( stream, cubes[i].interacting );
            serialize_vector( stream, cubes[i].position );
            serialize_quaternion( stream, cubes[i].orientation );
        }
    }

    CubeState cubes[NumCubes];
};

struct SnapshotAckPacket : public protocol::Packet
{
    uint16_t ack;

    SnapshotAckPacket() : Packet( SNAPSHOT_ACK_PACKET )
    {
        ack = 0;
    }

    PROTOCOL_SERIALIZE_OBJECT( stream )
    {
        serialize_uint16( stream, ack );
    }
};

class SnapshotPacketFactory : public protocol::PacketFactory
{
    core::Allocator * m_allocator;

public:

    SnapshotPacketFactory( core::Allocator & allocator )
        : PacketFactory( allocator, SNAPSHOT_NUM_PACKETS )
    {
        m_allocator = &allocator;
    }

protected:

    protocol::Packet * CreateInternal( int type )
    {
        switch ( type )
        {
            case SNAPSHOT_NAIVE_PACKET:     return CORE_NEW( *m_allocator, SnapshotNaivePacket );
            case SNAPSHOT_ACK_PACKET:       return CORE_NEW( *m_allocator, SnapshotAckPacket );
            default:
                return nullptr;
        }
    }
};

struct SnapshotInternal
{
    SnapshotInternal( core::Allocator & allocator ) 
        : packet_factory( allocator )
    {
        this->allocator = &allocator;
        network::SimulatorConfig networkSimulatorConfig;
        networkSimulatorConfig.packetFactory = &packet_factory;
        network_simulator = CORE_NEW( allocator, network::Simulator, networkSimulatorConfig );
        network_simulator->AddState( { Latency, Jitter, PacketLoss } );
        send_sequence = 0;
        mode = SnapshotDefaultMode;
    }

    ~SnapshotInternal()
    {
        CORE_ASSERT( network_simulator );
        typedef network::Simulator NetworkSimulator;
        CORE_DELETE( *allocator, NetworkSimulator, network_simulator );
        network_simulator = nullptr;
    }

    core::Allocator * allocator;
    SnapshotPacketFactory packet_factory;
    network::Simulator * network_simulator;
    uint16_t send_sequence;
    SnapshotMode mode;
};

SnapshotDemo::SnapshotDemo( core::Allocator & allocator )
{
    m_allocator = &allocator;
    m_internal = nullptr;
    m_settings = CORE_NEW( *m_allocator, CubesSettings );
    m_snapshot = CORE_NEW( *m_allocator, SnapshotInternal, *m_allocator );
}

SnapshotDemo::~SnapshotDemo()
{
    Shutdown();
    CORE_DELETE( *m_allocator, SnapshotInternal, m_snapshot );
    CORE_DELETE( *m_allocator, CubesSettings, m_settings );
    m_settings = nullptr;
    m_allocator = nullptr;
}

bool SnapshotDemo::Initialize()
{
    if ( m_internal )
        Shutdown();

    m_internal = CORE_NEW( *m_allocator, CubesInternal );    

    CubesConfig config;
    
    config.num_simulations = 1;
    config.num_views = 2;

    m_internal->Initialize( *m_allocator, config, m_settings );

    return true;
}

void SnapshotDemo::Shutdown()
{
    CORE_ASSERT( m_allocator );
    if ( m_internal )
    {
        m_internal->Free( *m_allocator );
        CORE_DELETE( *m_allocator, CubesInternal, m_internal );
        m_internal = nullptr;
    }
}

void SnapshotDemo::Update()
{
    CubesUpdateConfig update_config;

    auto local_input = m_internal->GetLocalInput();

    // setup left simulation to update one frame with local input

    update_config.sim[0].num_frames = 1;
    update_config.sim[0].frame_input[0] = local_input;

    // send a snapshot packet to the right simulation

    auto snapshot_packet = (SnapshotNaivePacket*) m_snapshot->packet_factory.Create( SNAPSHOT_NAIVE_PACKET );

    snapshot_packet->sequence = m_snapshot->send_sequence++;

    // todo: fill the packet

    m_snapshot->network_simulator->SendPacket( network::Address( "::1", RightPort ), snapshot_packet );

    // update the network simulator

    m_snapshot->network_simulator->Update( global.timeBase );

    // receive packets from the simulator (with latency, packet loss and jitter applied...)

    while ( true )
    {
        auto packet = m_snapshot->network_simulator->ReceivePacket();
        if ( !packet )
            break;

        auto port = packet->GetAddress().GetPort();
        auto type = packet->GetType();

        // IMPORTANT: Make sure we actually serialize read/write the packet.
        // Otherwise we're just passing around pointers to structs. LAME! :D

        uint8_t buffer[MaxPacketSize];

        protocol::WriteStream write_stream( buffer, MaxPacketSize );
        packet->SerializeWrite( write_stream );
        write_stream.Flush();
        CORE_CHECK( !write_stream.IsOverflow() );

        m_snapshot->packet_factory.Destroy( packet );
        packet = nullptr;

        protocol::ReadStream read_stream( buffer, MaxPacketSize );
        auto read_packet = m_snapshot->packet_factory.Create( type );
        read_packet->SerializeRead( read_stream );
        CORE_CHECK( !read_stream.IsOverflow() );

        if ( type == SNAPSHOT_NAIVE_PACKET && port == RightPort )
        {
            // naive snapshot packet for right simulation

            auto snapshot_packet = (SnapshotNaivePacket*) read_packet;

            // todo: apply the snapshot to the right simulation immediately

            if ( !snapshot_packet )
            {
                printf( "ohai\n" );
            }
        }

        m_snapshot->packet_factory.Destroy( read_packet );
        read_packet = nullptr;
    }


    // run the simulation

    m_internal->Update( update_config );
}

bool SnapshotDemo::Clear()
{
    return m_internal->Clear();
}

void SnapshotDemo::Render()
{
    CubesRenderConfig render_config;

    render_config.render_mode = CUBES_RENDER_SPLITSCREEN;

    m_internal->Render( render_config );
}

bool SnapshotDemo::KeyEvent( int key, int scancode, int action, int mods )
{
    if ( action == GLFW_PRESS && mods == 0 )
    {
        // todo: sta
        if ( key == GLFW_KEY_BACKSPACE )
        {
            Shutdown();
            Initialize();
            return true;
        }
    }

    return m_internal->KeyEvent( key, scancode, action, mods );
}

bool SnapshotDemo::CharEvent( unsigned int code )
{
    // ...

    return false;
}

#endif // #ifdef CLIENT
