# pip install --user fabric
# fab -f scripts/deploy.py deploy

from fabric.api import env,run,put,cd

env.hosts = [
    'nalland2@fa17-cs425-g13-01.cs.illinois.edu',
    'nalland2@fa17-cs425-g13-02.cs.illinois.edu'
#    'nalland2@fa17-cs425-g13-03.cs.illinois.edu'
#    'nalland2@fa17-cs425-g13-04.cs.illinois.edu',
#    'nalland2@fa17-cs425-g13-05.cs.illinois.edu',
#    'nalland2@fa17-cs425-g13-06.cs.illinois.edu',
#    'nalland2@fa17-cs425-g13-07.cs.illinois.edu',
#    'nalland2@fa17-cs425-g13-08.cs.illinois.edu',
#    'nalland2@fa17-cs425-g13-09.cs.illinois.edu',
#    'nalland2@fa17-cs425-g13-10.cs.illinois.edu'
]

def vm_str_num(host):
    if host[15] == '1':
        return str(10)
    else:
        return str(int(host[16]))

def deploy():
    run('mkdir -p ~/mp1/out')
    put(local_path='include', remote_path='~/mp1')
    put(local_path='src', remote_path='~/mp1')
    put(local_path='modules', remote_path='~/mp1')
    put(local_path='Makefile', remote_path='~/mp1')
    put(local_path='main.cpp', remote_path='~/mp1')
    put(local_path='main', remote_path='~/mp1')
    put(local_path='client_main', remote_path='~/mp1')
    put(local_path='server_main', remote_path='~/mp1')

#    if(vm_str_num(env.host) == "1"):
#        put(local_path='Tests', remote_path='~/mp1')

#    put(local_path='out/vm' + vm_str_num(env.host) + '.log', remote_path='~/mp1/out')
    with cd('~/mp1'):
        run('make workspace')
        run('make')
        run('find ! -name \".\" -name \".*\" -exec rm {} \;')
