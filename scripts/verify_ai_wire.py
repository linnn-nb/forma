#!/usr/bin/env python3
"""TEST DOUBLE ONLY: HTTP/tool-schema contract, never model E2E acceptance."""
from http.server import HTTPServer,BaseHTTPRequestHandler
from pathlib import Path
import json,subprocess,threading,tempfile,os,shutil
root=Path(__file__).resolve().parent.parent
original=Path((root/'evidence/current-test-project.txt').read_text().strip())
messages=[]
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):self.reply({'models':[{'name':'wire-fixture-only'}]})
 def do_POST(self):
  j=json.loads(self.rfile.read(int(self.headers.get('Content-Length',0))))
  messages.append({'route':self.path,'request':j})
  if self.path=='/api/show':return self.reply({'capabilities':['completion','tools'],'details':{'test_double':True}})
  if not any(m['role']=='tool' for m in j['messages']):
   function={'name':'query_session','arguments':{}}
  else:
   facts=json.loads(next(m['content'] for m in j['messages'] if m['role']=='tool'))
   assert 'path' not in facts['sources'][0]
   target=facts['tracks'][0]['id']
   op={'command':'set_track_gain','track_id':target,'gain_db':-21}
   if self.server.case=='denied':op={'command':'set_track_lock','track_id':target,'locked':False}
   function={'name':'propose_edit','arguments':{'operations':[op]}}
  self.reply({'done':True,'eval_count':20,'message':{'role':'assistant','content':'','tool_calls':[{'function':function}]}})
 def reply(self,j):
  data=json.dumps(j).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
server=HTTPServer(('127.0.0.1',0),Handler)
thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
results=[]
try:
 with tempfile.TemporaryDirectory(prefix='ndaw-wire-') as folder:
  p=Path(folder);project=p/'session.ndaw';shutil.copyfile(original,project)
  config=p/'provider.json';config.write_text(json.dumps({'kind':'ollama','endpoint':f'http://127.0.0.1:{server.server_port}','model':'wire-fixture-only'}))
  before=project.read_bytes()
  for case in ['valid','denied']:
   server.case=case
   r=subprocess.run([str(root/'build/ndaw_artefacts/Release/ndaw'),'ai-plan',str(project),'TEST FIXTURE: change track gain'],capture_output=True,text=True,env=dict(os.environ,NATIVEDAW_PROVIDER_CONFIG=str(config)),timeout=15)
   j=json.loads(r.stdout or r.stderr)
   assert (r.returncode==0 and j['status']=='awaiting_acceptance') if case=='valid' else (r.returncode==2 and j['code']=='permission')
   if case=='valid':
    assert j['evidence'][0]['model_digest'] is None  # Fixture supplied no weight digest; metadata hash must not impersonate it.
    assert len(j['evidence'][0]['model_metadata_sha256'])==64
   assert project.read_bytes()==before
   results.append({'case':case,'exit':r.returncode,'receipt':j,'session_file_unchanged':True})
finally:server.shutdown();server.server_close();thread.join()
report={'status':'passed_test_double_http_contract','real_model':False,'production_provider':False,'purpose':'exercise real HTTP adapter/schema/query/proposal validation only','results':results,'request_count':len(messages)}
(root/'evidence/ai-wire-contract.json').write_text(json.dumps(report,indent=2))
print(json.dumps({'status':report['status'],'real_model':False,'cases':len(results)}))
