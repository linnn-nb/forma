#!/usr/bin/env python3
"""Extract complete named receipts from preserved LastTest logs, not truncated XML.
CTest JUnit establishes group exit status; full JSON retains individual evidence.
"""
from pathlib import Path
import json,re,sys,xml.etree.ElementTree as ET
root=Path(sys.argv[1]);suffix=sys.argv[2] if len(sys.argv)>2 else 'final'
summary={}
for variant in ('release','audit','sanitize'):
 text=(root/f'ctest-{variant}-full.log').read_text()
 nodes=list(ET.parse(root/f'ctest-{variant}-{suffix}.xml').getroot().iter('testcase'))
 chunks=re.findall(r'Output:\n-+\n(.*?)\n<end of output>',text,re.S)
 assert len(chunks)==len(nodes),(variant,'missing complete group output')
 groups=[]
 for index,(chunk,node) in enumerate(zip(chunks,nodes),1):
  data=None
  for match in re.finditer(r'\{',chunk):
   try:candidate,_=json.JSONDecoder().raw_decode(chunk[match.start():])
   except json.JSONDecodeError:continue
   if isinstance(candidate,dict) and ('results' in candidate or 'checks' in candidate or node.attrib['name']=='rt_audit_negative_control'):
    data=candidate;break
  assert data is not None and not node.findall('failure') and not node.findall('error'),(variant,node.attrib['name'])
  checks=[r for r in data.get('results',data.get('checks',[])) if r.get('status') in ('passed','failed') or 'passed' in r]
  assert all(r.get('passed',r.get('status')=='passed') for r in checks) and data.get('failed',0)==0
  negative=node.attrib['name']=='rt_audit_negative_control'
  if negative:assert data['status']=='passed' and all(data['counts'][k]>0 for k in ('allocation','free','blocking_lock_wait','file_network','device_property_control'))
  else:assert checks,'No actual named checks'
  file=root/f'test-{variant}-group-{index}.json';file.write_text(json.dumps(data,indent=2))
  groups.append({'name':node.attrib['name'],'receipt':str(file),'named_checks':len(checks),'passed':True,'seconds':node.attrib.get('time'),'auxiliary_negative_control':negative})
 summary[variant]={'groups':groups,'named_checks':sum(g['named_checks'] for g in groups if not g['auxiliary_negative_control']),'all_passed':True}
(root/'test-builds-summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps({v:d['named_checks'] for v,d in summary.items()}))
