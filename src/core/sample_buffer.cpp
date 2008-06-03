#ifndef SINGLE_SOURCE_COMPILE

/*
 * sample_buffer.cpp - container-class sampleBuffer
 *
 * Copyright (c) 2005-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * 
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */


#include "sample_buffer.h"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QtCore/QBuffer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QPainter>


#include <cstring>

#ifdef SDL_SDL_SOUND_H
#include SDL_SDL_SOUND_H
#endif

#ifdef HAVE_SNDFILE_H
#include <sndfile.h>
#endif

#ifdef HAVE_VORBIS_VORBISFILE_H
#include <vorbis/vorbisfile.h>
#endif

#ifdef HAVE_FLAC_STREAM_ENCODER_H
#include <FLAC/stream_encoder.h>
#endif

#ifdef HAVE_FLAC_STREAM_DECODER_H
#include <FLAC/stream_decoder.h>
#endif


#include "base64.h"
#include "config_mgr.h"
#include "debug.h"
#include "drumsynth.h"
#include "endian_handling.h"
#include "engine.h"
#include "interpolation.h"
#include "templates.h"



sampleBuffer::sampleBuffer( const QString & _audio_file,
							bool _is_base64_data ) :
	m_audioFile( ( _is_base64_data == TRUE ) ? "" : _audio_file ),
	m_origData( NULL ),
	m_origFrames( 0 ),
	m_data( NULL ),
	m_frames( 0 ),
	m_startFrame( 0 ),
	m_endFrame( 0 ),
	m_loop_startFrame( 0 ),
	m_loop_endFrame( 0 ),
	m_amplification( 1.0f ),
	m_reversed( FALSE ),
	m_frequency( BaseFreq ),
	m_sampleRate( engine::getMixer()->baseSampleRate() )
{
#ifdef SDL_SDL_SOUND_H
	// init sound-file-system of SDL
	Sound_Init();
#endif
	if( _is_base64_data == TRUE )
	{
		loadFromBase64( _audio_file );
	}
	update();
}




sampleBuffer::sampleBuffer( const sampleFrame * _data, const f_cnt_t _frames ) :
	m_audioFile( "" ),
	m_origData( NULL ),
	m_origFrames( 0 ),
	m_data( NULL ),
	m_frames( 0 ),
	m_startFrame( 0 ),
	m_endFrame( 0 ),
	m_loop_startFrame( 0 ),
	m_loop_endFrame( 0 ),
	m_amplification( 1.0f ),
	m_reversed( FALSE ),
	m_frequency( BaseFreq ),
	m_sampleRate( engine::getMixer()->baseSampleRate() )
{
	if( _frames > 0 )
	{
		m_origData = new sampleFrame[_frames];
		memcpy( m_origData, _data, _frames * BYTES_PER_FRAME );
		m_origFrames = _frames;
	}
#ifdef SDL_SDL_SOUND_H
	// init sound-file-system of SDL
	Sound_Init();
#endif
	update();
}




sampleBuffer::sampleBuffer( const f_cnt_t _frames ) :
	m_audioFile( "" ),
	m_origData( NULL ),
	m_origFrames( 0 ),
	m_data( NULL ),
	m_frames( 0 ),
	m_startFrame( 0 ),
	m_endFrame( 0 ),
	m_loop_startFrame( 0 ),
	m_loop_endFrame( 0 ),
	m_amplification( 1.0f ),
	m_reversed( FALSE ),
	m_frequency( BaseFreq ),
	m_sampleRate( engine::getMixer()->baseSampleRate() )
{
	if( _frames > 0 )
	{
		m_origData = new sampleFrame[_frames];
		memset( m_origData, 0, _frames * BYTES_PER_FRAME );
		m_origFrames = _frames;
	}
#ifdef SDL_SDL_SOUND_H
	// init sound-file-system of SDL
	Sound_Init();
#endif
	update();
}




sampleBuffer::~sampleBuffer()
{
	delete[] m_origData;
	delete[] m_data;
}






void sampleBuffer::update( bool _keep_settings )
{
	const bool lock = ( m_data == NULL );
	if( lock )
	{
		engine::getMixer()->lock();
		delete[] m_data;
	}

	if( m_audioFile == "" && m_origData != NULL && m_origFrames > 0 )
	{
		// TODO: reverse- and amplification-property is not covered
		// by following code...
		m_data = new sampleFrame[m_origFrames];
		memcpy( m_data, m_origData, m_origFrames * BYTES_PER_FRAME );
		if( _keep_settings == FALSE )
		{
			m_frames = m_origFrames;
			m_loop_startFrame = m_startFrame = 0;
			m_loop_endFrame = m_endFrame = m_frames;
		}
	}
	else if( m_audioFile != "" )
	{
		QString file = tryToMakeAbsolute( m_audioFile );
		char * f = qstrdup( file.toAscii().constData() );
		int_sample_t * buf = NULL;
		ch_cnt_t channels = DEFAULT_CHANNELS;
		sample_rate_t samplerate = engine::getMixer()->baseSampleRate();

		m_frames = 0;

#ifdef HAVE_SNDFILE_H
		if( m_frames == 0 )
		{
			m_frames = decodeSampleSF( f, buf, channels,
								samplerate );
		}
#endif
#ifdef HAVE_VORBIS_VORBISFILE_H
		if( m_frames == 0 )
		{
			m_frames = decodeSampleOGGVorbis( f, buf, channels,
								samplerate );
		}
#endif
#ifdef SDL_SDL_SOUND_H
		if( m_frames == 0 )
		{
			m_frames = decodeSampleSDL( f, buf, channels,
								samplerate );
		}
#endif
		if( m_frames == 0 )
		{
			m_frames = decodeSampleDS( f, buf, channels,
								samplerate );
		}

		delete[] f;

		if( m_frames > 0 && buf != NULL )
		{
			// following code transforms int-samples into
			// float-samples and does amplifying & reversing
			const float fac = m_amplification /
						OUTPUT_SAMPLE_MULTIPLIER;
			m_data = new sampleFrame[m_frames];

			// if reversing is on, we also reverse when
			// scaling
			if( m_reversed )
			{
				for( f_cnt_t frame = 0; frame < m_frames;
								++frame )
				{
					for( ch_cnt_t chnl = 0;
							chnl < DEFAULT_CHANNELS;
									++chnl )
					{
const f_cnt_t idx = ( m_frames - frame ) * channels + ( chnl % channels );
m_data[frame][chnl] = buf[idx] * fac;
					}
				}
			}
			else
			{
				for( f_cnt_t frame = 0; frame < m_frames;
								++frame )
				{
					for( ch_cnt_t chnl = 0;
							chnl < DEFAULT_CHANNELS;
									++chnl )
					{
		const f_cnt_t idx = frame * channels + ( chnl % channels );
		m_data[frame][chnl] = buf[idx] * fac;
					}
				}
			}

			delete[] buf;

			normalizeSampleRate( samplerate, _keep_settings );
		}
		else
		{
			// sample couldn't be decoded, create buffer containing
			// one sample-frame
			m_data = new sampleFrame[1];
			memset( m_data, 0, sizeof( *m_data ) );
			m_frames = 1;
			m_loop_startFrame = m_startFrame = 0;
			m_loop_endFrame = m_endFrame = 1;
		}
	}
	else
	{
		// neither an audio-file nor a buffer to copy from, so create
		// buffer containing one sample-frame
		m_data = new sampleFrame[1];
		memset( m_data, 0, sizeof( *m_data ) );
		m_frames = 1;
		m_loop_startFrame = m_startFrame = 0;
		m_loop_endFrame = m_endFrame = 1;
	}

	if( lock )
	{
		engine::getMixer()->unlock();
	}

	emit sampleUpdated();
}




void sampleBuffer::normalizeSampleRate( const sample_rate_t _src_sr,
							bool _keep_settings )
{
	// do samplerate-conversion to our default-samplerate
	if( _src_sr != engine::getMixer()->baseSampleRate() )
	{
		sampleBuffer * resampled = resample( this, _src_sr,
					engine::getMixer()->baseSampleRate() );
		delete[] m_data;
		m_frames = resampled->frames();
		m_data = new sampleFrame[m_frames];
		memcpy( m_data, resampled->data(), m_frames *
							sizeof( sampleFrame ) );
		delete resampled;
	}

	if( _keep_settings == FALSE )
	{
		// update frame-variables
		m_loop_startFrame = m_startFrame = 0;
		m_loop_endFrame = m_endFrame = m_frames;
	}
}




#ifdef SDL_SDL_SOUND_H
f_cnt_t sampleBuffer::decodeSampleSDL( const char * _f,
					int_sample_t * & _buf,
					ch_cnt_t _channels,
					sample_rate_t _samplerate )
{
	Sound_AudioInfo STD_AUDIO_INFO =
	{
		AUDIO_S16SYS,
		_channels,
		_samplerate
	} ;
	f_cnt_t frames = 0;

	Sound_Sample * snd_sample = Sound_NewSampleFromFile( _f,
						&STD_AUDIO_INFO, 16384 );
	// file found?
	if( snd_sample != NULL )
	{
		// let SDL_sound decode our file to requested format
		( void )Sound_DecodeAll( snd_sample );
		frames = snd_sample->buffer_size / ( BYTES_PER_INT_SAMPLE *
								_channels );
		_buf = new int_sample_t[frames * _channels];
		memcpy( _buf, snd_sample->buffer, snd_sample->buffer_size );

		Sound_FreeSample( snd_sample );
	}
	return( frames );
}
#endif




#ifdef HAVE_SNDFILE_H
f_cnt_t sampleBuffer::decodeSampleSF( const char * _f,
					int_sample_t * & _buf,
					ch_cnt_t & _channels,
					sample_rate_t & _samplerate )
{
	SNDFILE * snd_file;
	SF_INFO sf_info;
	f_cnt_t frames = 0;
#ifdef OLD_SNDFILE
	if( ( snd_file = sf_open_read( _f, &sf_info ) ) != NULL )
	{
		frames = sf_info.samples;
#else
	if( ( snd_file = sf_open( _f, SFM_READ, &sf_info ) ) != NULL )
	{
		frames = sf_info.frames;
#endif
		_buf = new int_sample_t[sf_info.channels * frames];
		if( sf_read_short( snd_file, _buf, sf_info.channels * frames )
						< sf_info.channels * frames )
		{
#ifdef DEBUG_LMMS
			printf( "sampleBuffer::decodeSampleSF(): could not read"
				" sample %s: %s\n", _f, sf_strerror( NULL ) );
#endif
		}
		_channels = sf_info.channels;
		_samplerate = sf_info.samplerate;

		sf_close( snd_file );
	}
	else
	{
#ifdef DEBUG_LMMS
		printf( "sampleBuffer::decodeSampleSF(): could not load "
				"sample %s: %s\n", _f, sf_strerror( NULL ) );
#endif
	}
	return( frames );
}
#endif




#ifdef HAVE_VORBIS_VORBISFILE_H

// callback-functions for reading ogg-file

size_t qfileReadCallback( void * _ptr, size_t _size, size_t _n, void * _udata )
{
	return( static_cast<QFile *>( _udata )->read( (char*) _ptr,
								_size * _n ) );
}




int qfileSeekCallback( void * _udata, ogg_int64_t _offset, int _whence )
{
	QFile * f = static_cast<QFile *>( _udata );

	if( _whence == SEEK_CUR )
	{
		f->seek( f->pos() + _offset );
	}
	else if( _whence == SEEK_END )
	{
		f->seek( f->size() + _offset );
	}
	else
	{
		f->seek( _offset );
	}
	return( 0 );
}




int qfileCloseCallback( void * _udata )
{
	delete static_cast<QFile *>( _udata );
	return( 0 );
}




long qfileTellCallback( void * _udata )
{
	return( static_cast<QFile *>( _udata )->pos() );
}




f_cnt_t sampleBuffer::decodeSampleOGGVorbis( const char * _f,
						int_sample_t * & _buf,
						ch_cnt_t & _channels,
						sample_rate_t & _samplerate )
{
	static ov_callbacks callbacks =
	{
		qfileReadCallback,
		qfileSeekCallback,
		qfileCloseCallback,
		qfileTellCallback
	} ;

	OggVorbis_File vf;

	f_cnt_t frames = 0;

	QFile * f = new QFile( _f );
	if( f->open( QFile::ReadOnly ) == FALSE )
	{
		delete f;
		return( 0 );
	}

	int err = ov_open_callbacks( f, &vf, NULL, 0, callbacks );

	if( err < 0 )
	{
		switch( err )
		{
			case OV_EREAD:
				printf( "sampleBuffer::decodeSampleOGGVorbis():"
						" media read error\n" );
				break;
			case OV_ENOTVORBIS:
/*				printf( "sampleBuffer::decodeSampleOGGVorbis():"
					" not an Ogg Vorbis file\n" );*/
				break;
			case OV_EVERSION:
				printf( "sampleBuffer::decodeSampleOGGVorbis():"
						" vorbis version mismatch\n" );
				break;
			case OV_EBADHEADER:
				printf( "sampleBuffer::decodeSampleOGGVorbis():"
					" invalid Vorbis bitstream header\n" );
				break;
			case OV_EFAULT:
				printf( "sampleBuffer::decodeSampleOgg(): "
					"internal logic fault\n" );
				break;
		}
		delete f;
		return( 0 );
	}

	ov_pcm_seek( &vf, 0 );

   	_channels = ov_info( &vf, -1 )->channels;
   	_samplerate = ov_info( &vf, -1 )->rate;

	ogg_int64_t total = ov_pcm_total( &vf, -1 );

	_buf = new int_sample_t[total * _channels];
	int bitstream = 0;
	long bytes_read = 0;

	do
	{
		bytes_read = ov_read( &vf, (char *) &_buf[frames * _channels],
					( total - frames ) * _channels *
							BYTES_PER_INT_SAMPLE,
					isLittleEndian() ? 0 : 1,
					BYTES_PER_INT_SAMPLE, 1, &bitstream );
		if( bytes_read < 0 )
		{
			break;
		}
		frames += bytes_read / ( _channels * BYTES_PER_INT_SAMPLE );
	}
	while( bytes_read != 0 && bitstream == 0 );

	ov_clear( &vf );

	return( frames );
}
#endif




f_cnt_t sampleBuffer::decodeSampleDS( const char * _f,
						int_sample_t * & _buf,
						ch_cnt_t & _channels,
						sample_rate_t & _samplerate )
{
	DrumSynth ds;
	return( ds.GetDSFileSamples( _f, _buf, _channels ) );
}




bool FASTCALL sampleBuffer::play( sampleFrame * _ab, handleState * _state,
					const fpp_t _frames,
					const float _freq,
					const bool _looped ) const
{
	engine::getMixer()->clearAudioBuffer( _ab, _frames );

	if( m_endFrame == 0 || _frames == 0 )
	{
		return( FALSE );
	}

	const double freq_factor = (double) _freq / (double) m_frequency *
		m_sampleRate / engine::getMixer()->processingSampleRate();

	// calculate how many frames we have in requested pitch
	const f_cnt_t total_frames_for_current_pitch = static_cast<f_cnt_t>( (
						m_endFrame - m_startFrame ) /
								freq_factor );
	if( total_frames_for_current_pitch == 0 )
	{
		return( FALSE );
	}

	// this holds the number of the first frame to play
	f_cnt_t play_frame = _state->m_frameIndex;
	if( play_frame < m_startFrame )
	{
		play_frame = m_startFrame;
	}

	// this holds the number of remaining frames in current loop
	f_cnt_t frames_for_loop;
	if( _looped )
	{
		play_frame = getLoopedIndex( play_frame );
		frames_for_loop = static_cast<f_cnt_t>(
					( m_loop_endFrame - play_frame ) /
								freq_factor );
	}
	else
	{
		if( play_frame >= m_endFrame )
		{
			return( FALSE );
		}
		frames_for_loop = static_cast<f_cnt_t>(
					( m_endFrame - play_frame ) /
								freq_factor );
		if( frames_for_loop == 0 )
		{
			return( FALSE );
		}
	}

	sampleFrame * tmp = NULL;

	// check whether we have to change pitch...
	if( freq_factor != 1.0 || _state->m_varyingPitch )
	{
		SRC_DATA src_data;
		// Generate output
		const f_cnt_t margin = 64;
		f_cnt_t fragment_size = (f_cnt_t)( _frames * freq_factor )
								+ margin;
		src_data.data_in = getSampleFragment( play_frame,
					fragment_size, _looped, &tmp )[0];
		src_data.data_out = _ab[0];
		src_data.input_frames = fragment_size;
		src_data.output_frames = _frames;
		src_data.src_ratio = 1.0 / freq_factor;
		src_data.end_of_input = _state->m_eof;
		int error = src_process( _state->m_resamplingData,
								&src_data );
		if( error )
		{
			printf( "sampleBuffer: error while resampling: %s\n",
							src_strerror( error ) );
		}
		if( src_data.output_frames_gen > _frames )
		{
			printf( "sampleBuffer: not enough frames: %ld / %d\n",
					src_data.output_frames_gen, _frames );
		}
		_state->m_eof = src_data.end_of_input;
		// Advance
		play_frame += src_data.input_frames_used;
		if( _looped )
		{
			play_frame = getLoopedIndex( play_frame );
		}
	}
	else
	{
		// we don't have to pitch, so we just copy the sample-data
		// as is into pitched-copy-buffer

		// Generate output
		memcpy( _ab,
			getSampleFragment( play_frame, _frames, _looped, &tmp ),
						_frames * BYTES_PER_FRAME );
		// Advance
		play_frame += _frames;
		if( _looped )
		{
			play_frame = getLoopedIndex( play_frame );
		}
	}

	delete[] tmp;

	_state->m_frameIndex = play_frame;

	return( TRUE );

}




sampleFrame * sampleBuffer::getSampleFragment( f_cnt_t _start,
		f_cnt_t _frames, bool _looped, sampleFrame * * _tmp ) const
{
	if( _looped )
	{
		if( _start + _frames <= m_loop_endFrame )
		{
			return( m_data + _start );
		}
	}
	else
	{
		if( _start + _frames <= m_endFrame )
		{
			return( m_data + _start );
		}
	}

	*_tmp = new sampleFrame[_frames];

	if( _looped )
	{
		f_cnt_t copied = m_loop_endFrame - _start;
		memcpy( *_tmp, m_data + _start, copied * BYTES_PER_FRAME );
		f_cnt_t loop_frames = m_loop_endFrame - m_loop_startFrame;
		while( _frames - copied > 0 )
		{
			f_cnt_t todo = tMin( _frames - copied, loop_frames );
			memcpy( *_tmp + copied, m_data + m_loop_startFrame,
						todo * BYTES_PER_FRAME );
			copied += todo;
		}
	}
	else
	{
		f_cnt_t available = m_endFrame - _start;
		memcpy( *_tmp, m_data + _start, available * BYTES_PER_FRAME );
		memset( *_tmp + available, 0, ( _frames - available ) *
							BYTES_PER_FRAME );
	}

	return( *_tmp );
}




f_cnt_t sampleBuffer::getLoopedIndex( f_cnt_t _index ) const
{
	if( _index < m_loop_endFrame )
	{
		return( _index );
	}
	return( m_loop_startFrame + ( _index - m_loop_startFrame )
				% ( m_loop_endFrame - m_loop_startFrame ) );
}




void sampleBuffer::visualize( QPainter & _p, const QRect & _dr,
							const QRect & _clip )
{
//	_p.setClipRect( _clip );
//	_p.setPen( QColor( 0x22, 0xFF, 0x44 ) );
	//_p.setPen( QColor( 64, 224, 160 ) );
#warning TODO: save and restore aa-settings
	_p.setRenderHint( QPainter::Antialiasing );
	const int w = _dr.width();
	const int h = _dr.height();

	const float y_base = h / 2 + _dr.y();
	const float y_space = h / 2;

	const QRect isect = _dr.intersect( _clip );

	int old_y[DEFAULT_CHANNELS] = { y_base, y_base };

	_p.setPen( QPen( _p.pen().color(), 0.5 ) );
	const f_cnt_t fpp = tLimit<f_cnt_t>( m_frames / w, 1, 20 );
	int old_x = _clip.x();
	for( f_cnt_t frame = 0; frame < m_frames; frame += fpp )
	{
		const float x = _dr.x() + ( frame /
					(float) m_frames * _dr.width() );
		for( ch_cnt_t chnl = 0; chnl < DEFAULT_CHANNELS; ++chnl )
		{
			const Uint16 y = y_base - (
						m_data[frame][chnl] * y_space );
			_p.drawLine( QLineF( old_x, old_y[chnl], x, y+0.5 ) );
			old_y[chnl] = y;
		}
		old_x = x;
	}
//	_p.setClipping( FALSE );
}




QString sampleBuffer::openAudioFile( void ) const
{
	QFileDialog ofd( NULL, tr( "Open audio file" ) );

	QString dir;
	if( m_audioFile != "" )
	{
		QString f = m_audioFile;
		if( QFileInfo( f ).isRelative() )
		{
			f = configManager::inst()->userSamplesDir() + f;
			if( QFileInfo( f ).exists() == FALSE )
			{
				f = configManager::inst()->factorySamplesDir() +
								m_audioFile;
			}
		}
		dir = QFileInfo( f ).absolutePath();
	}
	else
	{
		dir = configManager::inst()->userSamplesDir();
	}
	// change dir to position of previously opened file
	ofd.setDirectory( dir );
	ofd.setFileMode( QFileDialog::ExistingFiles );

	// set filters
	QStringList types;
	types << tr( "All Audio-Files (*.wav *.ogg *.ds *.flac *.spx *.voc "
					"*.aif *.aiff *.au *.raw *.mp3)" )
		<< tr( "Wave-Files (*.wav)" )
		<< tr( "OGG-Files (*.ogg)" )
		<< tr( "DrumSynth-Files (*.ds)" )
		<< tr( "FLAC-Files (*.flac)" )
		<< tr( "SPEEX-Files (*.spx)" )
		<< tr( "MP3-Files (*.mp3)" )
		//<< tr( "MIDI-Files (*.mid)" )
		<< tr( "VOC-Files (*.voc)" )
		<< tr( "AIFF-Files (*.aif *.aiff)" )
		<< tr( "AU-Files (*.au)" )
		<< tr( "RAW-Files (*.raw)" )
		//<< tr( "MOD-Files (*.mod)" )
		;
	ofd.setFilters( types );
	if( m_audioFile != "" )
	{
		// select previously opened file
		ofd.selectFile( QFileInfo( m_audioFile ).fileName() );
	}

	if( ofd.exec () == QDialog::Accepted )
	{
		if( ofd.selectedFiles().isEmpty() )
		{
			return( "" );
		}
		return( tryToMakeRelative( ofd.selectedFiles()[0] ) );
	}

	return( "" );
}


#undef HAVE_FLAC_STREAM_ENCODER_H	/* not yet... */
#undef HAVE_FLAC_STREAM_DECODER_H

#ifdef HAVE_FLAC_STREAM_ENCODER_H
FLAC__StreamEncoderWriteStatus flacStreamEncoderWriteCallback(
					const FLAC__StreamEncoder *
								/*_encoder*/,
					const FLAC__byte _buffer[], 
					unsigned int/* _samples*/,
					unsigned int _bytes,
					unsigned int/* _current_frame*/,
					void * _client_data )
{
/*	if( _bytes == 0 )
	{
		return( FLAC__STREAM_ENCODER_WRITE_STATUS_OK );
	}*/
	return( ( static_cast<QBuffer *>( _client_data )->write(
				(const char *) _buffer, _bytes ) ==
								(int) _bytes ) ?
				FLAC__STREAM_ENCODER_WRITE_STATUS_OK :
				FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR );
}


void flacStreamEncoderMetadataCallback( const FLAC__StreamEncoder *,
					const FLAC__StreamMetadata * _metadata,
					void * _client_data )
{
	QBuffer * b = static_cast<QBuffer *>( _client_data );
	b->seek( 0 );
	b->write( (const char *) _metadata, sizeof( *_metadata ) );
}

#endif



QString & sampleBuffer::toBase64( QString & _dst ) const
{
#ifdef HAVE_FLAC_STREAM_ENCODER_H
	const f_cnt_t FRAMES_PER_BUF = 1152;

	FLAC__StreamEncoder * flac_enc = FLAC__stream_encoder_new();
	FLAC__stream_encoder_set_channels( flac_enc, DEFAULT_CHANNELS );
	FLAC__stream_encoder_set_blocksize( flac_enc, FRAMES_PER_BUF );
/*	FLAC__stream_encoder_set_do_exhaustive_model_search( flac_enc, TRUE );
	FLAC__stream_encoder_set_do_mid_side_stereo( flac_enc, TRUE );*/
	FLAC__stream_encoder_set_sample_rate( flac_enc,
					engine::getMixer()->sampleRate() );
	QBuffer ba_writer;
	ba_writer.open( QBuffer::WriteOnly );

	FLAC__stream_encoder_set_write_callback( flac_enc,
					flacStreamEncoderWriteCallback );
	FLAC__stream_encoder_set_metadata_callback( flac_enc,
					flacStreamEncoderMetadataCallback );
	FLAC__stream_encoder_set_client_data( flac_enc, &ba_writer );
	if( FLAC__stream_encoder_init( flac_enc ) != FLAC__STREAM_ENCODER_OK )
	{
		printf( "error within FLAC__stream_encoder_init()!\n" );
	}
	f_cnt_t frame_cnt = 0;
	while( frame_cnt < m_frames )
	{
		f_cnt_t remaining = tMin<f_cnt_t>( FRAMES_PER_BUF,
							m_frames - frame_cnt );
		FLAC__int32 buf[FRAMES_PER_BUF * DEFAULT_CHANNELS];
		for( f_cnt_t f = 0; f < remaining; ++f )
		{
			for( ch_cnt_t ch = 0; ch < DEFAULT_CHANNELS; ++ch )
			{
				buf[f*DEFAULT_CHANNELS+ch] = (FLAC__int32)(
					mixer::clip( m_data[f+frame_cnt][ch] ) *
						OUTPUT_SAMPLE_MULTIPLIER );
			}
		}
		FLAC__stream_encoder_process_interleaved( flac_enc, buf,
								remaining );
		frame_cnt += remaining;
	}
	FLAC__stream_encoder_finish( flac_enc );
	FLAC__stream_encoder_delete( flac_enc );
	printf("%d %d\n", frame_cnt, (int)ba_writer.size() );
	ba_writer.close();

	base64::encode( ba_writer.buffer().data(), ba_writer.buffer().size(),
									_dst );


#else	/* HAVE_FLAC_STREAM_ENCODER_H */

	base64::encode( (const char *) m_data,
					m_frames * sizeof( sampleFrame ), _dst );

#endif	/* HAVE_FLAC_STREAM_ENCODER_H */

	return( _dst );
}




sampleBuffer * sampleBuffer::resample( sampleFrame * _data,
						const f_cnt_t _frames,
						const sample_rate_t _src_sr,
						const sample_rate_t _dst_sr )
{
	const f_cnt_t dst_frames = static_cast<f_cnt_t>( _frames /
					(float) _src_sr * (float) _dst_sr );
	sampleBuffer * dst_sb = new sampleBuffer( dst_frames );
	sampleFrame * dst_buf = dst_sb->m_origData;

	// yeah, libsamplerate, let's rock with sinc-interpolation!
	int error;
	SRC_STATE * state;
	if( ( state = src_new( SRC_SINC_MEDIUM_QUALITY,
					DEFAULT_CHANNELS, &error ) ) != NULL )
	{
		SRC_DATA src_data;
		src_data.end_of_input = 0;
		src_data.data_in = _data[0];
		src_data.data_out = dst_buf[0];
		src_data.input_frames = _frames;
		src_data.output_frames = dst_frames;
		src_data.src_ratio = (double) _dst_sr / _src_sr;
		int error;
		if( ( error = src_process( state, &src_data ) ) )
		{
			printf( "sampleBuffer: error while resampling: %s\n",
							src_strerror( error ) );
		}
		src_delete( state );
	}
	else
	{
		printf( "Error: src_new() failed in sample_buffer.cpp!\n" );
	}
	dst_sb->update();
	return( dst_sb );
}




void sampleBuffer::setAudioFile( const QString & _audio_file )
{
	m_audioFile = tryToMakeRelative( _audio_file );
	update();
}



#ifdef HAVE_FLAC_STREAM_DECODER_H

struct flacStreamDecoderClientData
{
	QBuffer * read_buffer;
	QBuffer * write_buffer;
} ;



FLAC__StreamDecoderReadStatus flacStreamDecoderReadCallback(
					const FLAC__StreamDecoder *
								/*_decoder*/,
					FLAC__byte * _buffer,
					unsigned int * _bytes,
					void * _client_data )
{
	int res = static_cast<flacStreamDecoderClientData *>(
					_client_data )->read_buffer->read(
						(char *) _buffer, *_bytes );

	if( res > 0 )
	{
		*_bytes = res;
		return( FLAC__STREAM_DECODER_READ_STATUS_CONTINUE );

	}
	*_bytes = 0;
	return( FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM );
}




FLAC__StreamDecoderWriteStatus flacStreamDecoderWriteCallback(
					const FLAC__StreamDecoder *
								/*_decoder*/,
					const FLAC__Frame * _frame,
					const FLAC__int32 * const _buffer[],
					void * _client_data )
{
	if( _frame->header.channels != 2 )
	{
		printf( "channels != 2 in "
					"flacStreamDecoderWriteCallback()\n" );
		return( FLAC__STREAM_DECODER_WRITE_STATUS_ABORT );
	}

	if( _frame->header.bits_per_sample != 16 )
	{
		printf( "bits_per_sample != 16 in "
					"flacStreamDecoderWriteCallback()\n" );
		return( FLAC__STREAM_DECODER_WRITE_STATUS_ABORT );
	}

	const f_cnt_t frames = _frame->header.blocksize;
	for( f_cnt_t frame = 0; frame < frames; ++frame )
	{
		sampleFrame sframe = { _buffer[0][frame] /
						OUTPUT_SAMPLE_MULTIPLIER,
					_buffer[1][frame] /
						OUTPUT_SAMPLE_MULTIPLIER
					} ;
		static_cast<flacStreamDecoderClientData *>(
					_client_data )->write_buffer->write(
				(const char *) sframe, sizeof( sframe ) );
	} 
	return( FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE );
}


void flacStreamDecoderMetadataCallback( const FLAC__StreamDecoder *,
					const FLAC__StreamMetadata *,
					void * /*_client_data*/ )
{
	printf("stream decoder metadata callback\n");
/*	QBuffer * b = static_cast<QBuffer *>( _client_data );
	b->seek( 0 );
	b->write( (const char *) _metadata, sizeof( *_metadata ) );*/
}


void flacStreamDecoderErrorCallback( const FLAC__StreamDecoder *,
					FLAC__StreamDecoderErrorStatus _status,
					void * /*_client_data*/ )
{
	printf("error callback! %d\n", _status);
	// what to do now??
}

#endif


void sampleBuffer::loadFromBase64( const QString & _data )
{
	char * dst = NULL;
	int dsize = 0;
	base64::decode( _data, &dst, &dsize );

#ifdef HAVE_FLAC_STREAM_DECODER_H

	QByteArray orig_data = QByteArray::fromRawData( dst, dsize );
	QBuffer ba_reader( &orig_data );
	ba_reader.open( QBuffer::ReadOnly );

	QBuffer ba_writer;
	ba_writer.open( QBuffer::WriteOnly );

	flacStreamDecoderClientData cdata = { &ba_reader, &ba_writer } ;

	FLAC__StreamDecoder * flac_dec = FLAC__stream_decoder_new();

	FLAC__stream_decoder_set_read_callback( flac_dec,
					flacStreamDecoderReadCallback );
	FLAC__stream_decoder_set_write_callback( flac_dec,
					flacStreamDecoderWriteCallback );
	FLAC__stream_decoder_set_error_callback( flac_dec,
					flacStreamDecoderErrorCallback );
	FLAC__stream_decoder_set_metadata_callback( flac_dec,
					flacStreamDecoderMetadataCallback );
	FLAC__stream_decoder_set_client_data( flac_dec, &cdata );

	FLAC__stream_decoder_init( flac_dec );

	FLAC__stream_decoder_process_until_end_of_stream( flac_dec );

	FLAC__stream_decoder_finish( flac_dec );
	FLAC__stream_decoder_delete( flac_dec );

	ba_reader.close();

	orig_data = ba_writer.buffer();
	printf("%d\n", (int) orig_data.size() );

	m_origFrames = orig_data.size() / sizeof( sampleFrame ); 
	delete[] m_origData;
	m_origData = new sampleFrame[m_origFrames];
	memcpy( m_origData, orig_data.data(), orig_data.size() );

#else /* HAVE_FLAC_STREAM_DECODER_H */

	m_origFrames = dsize / sizeof( sampleFrame ); 
	delete[] m_origData;
	m_origData = new sampleFrame[m_origFrames];
	memcpy( m_origData, dst, dsize );

#endif

	delete[] dst;

	m_audioFile = "";
	update();
}




void sampleBuffer::setStartFrame( const f_cnt_t _s )
{
	m_loop_startFrame = m_startFrame = _s;
}




void sampleBuffer::setEndFrame( const f_cnt_t _e )
{
	m_loop_endFrame = m_endFrame = _e;
}




void sampleBuffer::setAmplification( float _a )
{
	m_amplification = _a;
	update( TRUE );
}




void sampleBuffer::setReversed( bool _on )
{
	m_reversed = _on;
	update( TRUE );
}




QString sampleBuffer::tryToMakeRelative( const QString & _file )
{
	if( QFileInfo( _file ).isRelative() == FALSE )
	{
		QString fsd = configManager::inst()->factorySamplesDir();
		QString usd = configManager::inst()->userSamplesDir();
		if( _file.startsWith( fsd ) )
		{
			return( QString( _file ).mid( fsd.length() ) );
		}
		else if( _file.startsWith( usd ) )
		{
			return( QString( _file ).mid( usd.length() ) );
		}
	}
	return( _file );
}




QString sampleBuffer::tryToMakeAbsolute( const QString & _file )
{
	if( QFileInfo( _file ).isAbsolute() )
	{
		return( _file );
	}

	QString f = configManager::inst()->userSamplesDir() + _file;
	if( QFileInfo( f ).exists() )
	{
		return( f );
	}

	return( configManager::inst()->factorySamplesDir() + _file );
}








sampleBuffer::handleState::handleState( bool _varying_pitch ) :
	m_frameIndex( 0 ),
	m_varyingPitch( _varying_pitch ),
	m_eof( 0 )
{
	int error;
	if( ( m_resamplingData = src_new(/*
		( engine::getMixer()->highQuality() == TRUE ) ?
					SRC_SINC_FASTEST :*/
					SRC_LINEAR,
					DEFAULT_CHANNELS, &error ) ) == NULL )
	{
		printf( "Error: src_new() failed in sample_buffer.cpp!\n" );
	}
}




sampleBuffer::handleState::~handleState()
{
	src_delete( m_resamplingData );
}




#include "sample_buffer.moc"


#endif
