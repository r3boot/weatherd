
from __future__     import with_statement
from fabric.api 	import local, run, cd, settings, env

def sync(name='soekris'):
    local('rsync -avl --progress --exclude \"*.swp\" /people/r3boot/Projects/weatherd %s:' % name)
    #with settings(host_string=name):
    #    with cd('/home/r3boot/weatherd'):
    #        run('sudo ./weatherd.py')
