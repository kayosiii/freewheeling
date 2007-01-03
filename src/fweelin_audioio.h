#ifndef __FWEELIN_AUDIOIO_H
#define __FWEELIN_AUDIOIO_H

extern "C"
{
#include <jack/jack.h>
}

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

class Fweelin;
class Processor;

// **************** SYSTEM LEVEL AUDIO

class AudioIO {
public:
  AudioIO(Fweelin *app) : sync_type(0), sync_speed(1), sync_start_frame(0), 
    timebase_master(0), sync_active(0), app(app) {};

  // Open up system level audio
  int open ();

  // Activate system level audio
  int activate (Processor *rp);

  // Close system level audio
  void close ();

  // Get realtime buffer size
  nframes_t getbufsz();

  // **Callbacks**

  // Realtime process function.. the beginning of the DSP chain
  static int process (nframes_t nframes, void *arg);

  // Timebase (jack transport sync) callback
  static void timebase_callback(jack_transport_state_t state, 
				jack_nframes_t nframes, 
				jack_position_t *pos, int new_pos, void *arg);
    
  // Sampling rate change callback
  static int srate_callback (nframes_t nframes, void *arg);

  // Callback for audio shutdown
  static void audio_shutdown (void *arg);

  // Get current sampling rate
  inline nframes_t get_srate() { return srate; };

  // Get approximate audio CPU load
  inline float GetAudioCPULoad() { return cpuload; };
  
  inline float GetTimeScale() { return timescale; };
  
  // Transport sync methods

  // Reposition transport to the given position
  // Used for syncing external apps
  void RelocateTransport(nframes_t pos);

  // Get current bar in transport mechanism
  inline int GetTransport_Bar() { return jpos.bar; };

  // Get current beat in transport mechaniasm
  inline int GetTransport_Beat() { return jpos.beat; };

  // Get current BPM in transport mechanism
  inline double GetTransport_BPM() { return jpos.beats_per_minute; };

  // Get current # of beats per bar in transport mechanism
  inline float GetTransport_BPB() { return jpos.beats_per_bar; };

  inline int GetTransport_SyncSpeed() { return sync_speed; };
  inline char GetTransport_SyncType() { return sync_type; };

  inline void SetTransport_SyncSpeed(int sspd) { 
    if (sspd < 1)
      sync_speed = 1;
    else
      sync_speed = sspd; 
  };
  inline void SetTransport_SyncType(char stype) { sync_type = stype; };

  // Are we sending or receiving sync?
  inline char IsSync() { return sync_active; };

  // Are we the timebase master?
  inline char IsTimebaseMaster() { return timebase_master; };
  
  // Is the transport rolling?
  inline char IsTransportRolling() { return transport_roll; };

  // Audio system client
  jack_client_t *client;

  // Inputs and outputs- stereo pairs
  jack_port_t **iport[2], **oport[2];

  float cpuload; // Current approximate audio CPU load
  float timescale; // fragment length/sample rate = length (s) of one fragment
  nframes_t srate; // Sampling rate

  // Variables for sync

  char sync_type; // Nonzero for beat-sync and zero for bar-sync
  int sync_speed; // Number of beats or bars (external) per pulse (FW)

  // Jack frame where the first bar began in transport
  int32_t sync_start_frame; 
  char repos; // Nonzero if we have repositioned JACK internally
  jack_position_t jpos; // Current JACK position
  char timebase_master; // Nonzero if we are the JACK timebase master
  char sync_active;     // Nonzero if sync is active
  char transport_roll;  // Nonzero if the transport is rolling

  // Pointer to the main app
  Fweelin *app;
  // Processor which is basically the root of the signal flow
  Processor *rp; 
};

#endif