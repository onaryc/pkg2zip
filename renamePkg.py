#!/usr/bin/env python2

import os
from os import listdir
from os.path import isfile, join, splitext
import subprocess
#import os.path

def renamePkg(source):
    command = ['renamePkg.exe']

    command.append(source)

    try:
        subprocess.call(command)
    except :
        print 'renamePkg.exe error'

def listPkgFiles(baseDirectory = '.'):
    pkgFiles = []
    
    ## get all files, dir from baseDirectory
    listFiles = os.listdir(baseDirectory)
    for filename in listFiles:
        if isfile(join(baseDirectory, filename)):
            ## check the extension
            basename, extension = os.path.splitext(filename)
            
            if extension == '.pkg':
                pkgFiles.append(filename)

    return pkgFiles
    
def main ():
    pkgFiles = listPkgFiles()

    print 'pkgFiles', pkgFiles
    for pkgFile in pkgFiles:
        print 'Renaming ' + pkgFile + '\n'
        renamePkg(pkgFile)

if __name__ == "__main__":
    main ()
