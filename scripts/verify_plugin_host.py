#!/usr/bin/env python3
"""Real installed AU effect/VST3 inspection and decoded offline/state checks.
Known PCM only supplies a numerical input; actual production plugin SDK executes.
No sample/library/plugin binaries or opaque state enter committed evidence.
"""
import array,hashlib,json,math,struct,subprocess,sys,uuid,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if len(sys.argv) not in (2,3,4):raise SystemExit('Usage: verify_plugin_host.py REAL_MUSIC_WAV [ABSOLUTE_CLI] [EVIDENCE_PREFIX]')
music=Path(sys.argv[1]).resolve();cli=Path(sys.argv[2]).resolve() if len(sys.argv)>=3 else ROOT/'build/ndaw_artefacts/Release/ndaw'
prefix=sys.argv[3] if len(sys.argv)==4 else 'plugins-real-host'
assert prefix and all(c.isalnum() or c in '-_' for c in prefix)
run=ROOT/'evidence/runs'/('plugin-host-'+str(uuid.uuid4()));run.mkdir(parents=True);catalog=run/'catalog'
summary={'status':'running','binary':str(cli),'run_directory':str(run),'real_plugins':True,'real_model':False,'listening':False,
 'scope':'isolated discovery/inspection and actual Apple AU offline/state audio; no live project inserts/realtime IPC/editor/MIDI/automation/ARA acceptance','checks':[]}
def hashed(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def call(label,*args,expected=0):
 r=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,timeout=13)
 (run/(label+'.stdout.json')).write_text(r.stdout);(run/(label+'.stderr.txt')).write_text(r.stderr)
 payload=(r.stdout or r.stderr).lstrip();start=payload.find('{')
 if start<0:raise AssertionError(f'{label} returned no structured receipt, exit {r.returncode}')
 p,_=json.JSONDecoder().raw_decode(payload[start:])
 if r.returncode!=expected:raise AssertionError(f'{label} expected exit {expected}, got {r.returncode}: {p.get("code",p.get("status"))}')
 return p
def pcm(p):
 raw=p.read_bytes();assert raw[:4]==b'RIFF' and raw[8:12]==b'WAVE';at=12;fmt=data=None
 while at+8<=len(raw):
  kind,size=struct.unpack_from('<4sI',raw,at);chunk=raw[at+8:at+8+size]
  if kind==b'fmt ':fmt=chunk
  if kind==b'data':data=chunk
  at+=8+size+(size&1)
 assert fmt is not None and data is not None
 code,channels,rate,_,align,bits=struct.unpack_from('<HHIIHH',fmt)
 if code==0xfffe:code=struct.unpack_from('<I',fmt,24)[0]
 assert code==3 and bits==32 and channels==2 and align==8
 samples=array.array('f');samples.frombytes(data)
 if sys.byteorder!='little':samples.byteswap()
 assert all(math.isfinite(x) for x in samples)
 return samples,rate,len(samples)//channels
def component(samples,frequency,rate,start=12000):
 count=len(samples)//2-start;c=s=0.0
 for n in range(start,len(samples)//2):
  angle=2*math.pi*frequency*n/rate;value=samples[2*n];c+=value*math.cos(angle);s+=value*math.sin(angle)
 return 2*math.hypot(c,s)/count
try:
 discovered=call('discovery','plugins-discover',catalog)
 candidates=discovered['response']['candidates'];assert {'format':'AudioUnit','candidate':'AudioUnit:Effects/aufx,lpas,appl','scan_verified':False} in candidates
 au=call('au-scan','plugins-scan','AudioUnit','AudioUnit:Effects/aufx,lpas,appl',catalog)
 vst=call('vst3-scan','plugins-scan','VST3','/Library/Audio/Plug-Ins/VST3/Serum.vst3',catalog)
 assert au['status']==vst['status']=='verified'
 ap=au['entry']['plugins'][0];vp=vst['entry']['plugins'][0];parameters={p['id']:p for p in ap['parameters']}
 assert parameters['0']['name']=='Cutoff Frequency' and parameters['1']['name']=='Resonance'
 assert all(p['process']['reaped'] and p['process']['exit_code']==0 and p['process']['signal']==0 for p in (au['result'],vst['result']))
 summary['checks'].append({'id':'PLUGIN-REAL-01','passed':True,'candidates':len(candidates),
  'plugins':[{'id':p['id'],'name':p['name'],'format':p['format'],'version':p['version'],'parameters':len(p['parameters']),
              'buses':p['buses'],'opaque_state_bytes':p['opaque_state']['bytes'],'sdk_latency':p['reported_latency_frames']} for p in (ap,vp)],
  'processes':[au['result']['process'],vst['result']['process']]})
 fixture=run/'two-tone.wav';known=[]
 with wave.open(str(fixture),'wb') as w:
  w.setparams((2,2,48000,96000,'NONE','not compressed'));values=array.array('h')
  for n in range(96000):
   value=int(32767*(.04*math.sin(2*math.pi*200*n/48000)+.04*math.sin(2*math.pi*8000*n/48000)))
   values.extend((value,value));known.extend((value/32768,value/32768))
  if sys.byteorder!='little':values.byteswap()
  w.writeframes(values.tobytes())
 original=hashed(fixture);changes=run/'parameters.json';changes.write_text(json.dumps([{'id':'0','normalized':.05},{'id':'1','normalized':parameters['1']['value']}]))
 filtered=run/'filtered.wav';result=call('au-process','plugin-process-file',catalog,ap['id'],fixture,filtered,changes)
 assert result['status']=='succeeded';samples,rate,frames=pcm(filtered);assert rate==48000 and frames==96000 and hashed(fixture)==original
 gains={str(f):20*math.log10(component(samples,f,rate)/component(known,f,rate)) for f in (200,8000)}
 assert abs(gains['200'])<=1 and gains['8000']<=-25,gains
 after={p['id']:p for p in result['response']['parameters_after_processing']};assert abs(after['0']['value']-.05)<=1e-6
 restored=run/'restored.wav';none=run/'no-changes.json';none.write_text('[]')
 state_id=result['retained_state_id'];repeated=call('au-restore','plugin-process-file',catalog,ap['id'],fixture,restored,none,state_id)
 other,rr,rf=pcm(restored);assert rr==rate and rf==frames
 restore_params={p['id']:p for p in repeated['response']['parameters_after_restore']};assert abs(restore_params['0']['value']-.05)<=1e-6
 difference=max(abs(a-b) for a,b in zip(samples,other));assert difference<=1e-7,difference
 summary['checks'].append({'id':'PLUGIN-REAL-02','passed':True,'input_fixture':'known PCM16 test input only',
   'actual_processor':ap['name'],'input_sha256':original,'output_sha256':hashed(filtered),'output_rate':rate,'output_frames':frames,
   'frequency_gain_db':gains,'cutoff_display_from_sdk':after['0']['display'],'retained_state_id':state_id,'fresh_process_pcm_max_difference':difference,
   'process':result['process'],'processing_blocks':result['response']['processing_blocks'],'process_p99_us':result['response']['process_p99_us'],
   'tail_policy':result['response']['export']['tail_policy']})
 music_hash=hashed(music);rendered=run/'actual-music-filtered.wav';actual=call('actual-music-process','plugin-process-file',catalog,ap['id'],music,rendered,changes)
 music_pcm,mr,mf=pcm(rendered);assert actual['status']=='succeeded' and hashed(music)==music_hash and max(abs(x) for x in music_pcm)>1e-6
 assert mf==actual['response']['export']['format']['frames']
 summary['checks'].append({'id':'PLUGIN-REAL-03','passed':True,'actual_music_path':str(music),'original_sha256':music_hash,'original_unchanged':True,
  'export':actual['response']['export'],'process':actual['process'],'process_p99_us':actual['response']['process_p99_us'],'process_max_us':actual['response']['process_max_us']})
 bad=run/'invented-parameter.json';bad.write_text('[{"id":"warmth","normalized":0.5}]');failed_output=run/'rejected.wav'
 failure=call('invalid-parameter','plugin-process-file',catalog,ap['id'],fixture,failed_output,bad,expected=2)
 assert failure['status']=='failed' and failure['code']=='plugin_parameter' and not failed_output.exists()
 overwrite=call('overwrite-rejected','plugin-process-file',catalog,ap['id'],fixture,filtered,none,expected=2)
 assert overwrite['code']=='export_exists' and hashed(filtered)==result['response']['export']['format']['sha256']
 instrument=call('instrument-rejected','plugin-process-file',catalog,vp['id'],fixture,run/'instrument-rejected.wav',none,expected=2)
 assert instrument['status']=='failed' and instrument['code']=='plugin_instrument' and not (run/'instrument-rejected.wav').exists()
 unavailable_state=call('unknown-state-rejected','plugin-process-file',catalog,ap['id'],fixture,run/'state-rejected.wav',none,'unrecorded-state',expected=2)
 assert unavailable_state['code']=='plugin_state' and not (run/'state-rejected.wav').exists()
 invalid_pcm=run/'nonfinite-input.wav';raw=bytearray(filtered.read_bytes());at=12
 while at+8<=len(raw):
  kind,size=struct.unpack_from('<4sI',raw,at)
  if kind==b'data':struct.pack_into('<f',raw,at+8,float('nan'));break
  at+=8+size+(size&1)
 else:raise AssertionError('Missing actual float32 data chunk')
 invalid_pcm.write_bytes(raw);invalid_hash=hashed(invalid_pcm)
 invalid=call('nonfinite-rejected','plugin-process-file',catalog,ap['id'],invalid_pcm,run/'nonfinite-rejected.wav',none,expected=2)
 assert invalid['code']=='plugin_source_nonfinite' and invalid['external_output']['staging_retained'] and not (run/'nonfinite-rejected.wav').exists()
 assert hashed(invalid_pcm)==invalid_hash
 assert hashed(fixture)==original and hashed(music)==music_hash
 summary['checks'].append({'id':'PLUGIN-REAL-04','passed':True,'invented_parameter_rejected':True,'overwrite_rejected':True,'instrument_without_midi_rejected':True,'unknown_state_rejected':True,
   'known_nonfinite_input_rejected':True,'failed_partial_retained_and_reported':True,'original_unchanged':True})
 summary['status']='passed'
except Exception as e:
 summary['status']='failed';summary['error']=str(e);raise
finally:
 (run/'summary.json').write_text(json.dumps(summary,indent=2));(ROOT/'evidence'/(prefix+'.json')).write_text(json.dumps(summary,indent=2))
 print(json.dumps({'status':summary['status'],'checks':len(summary['checks']),'run':str(run),'error':summary.get('error')}),flush=True)
