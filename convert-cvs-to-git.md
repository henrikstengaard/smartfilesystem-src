# Convert CVS repo to Git

The procedure I used to convert the repo ``smartfilesystem`` from CVS to Git.

Based on http://www.embecosm.com/appnotes/ean11/ean11-howto-cvs-git-1.0.html.

## cvs repo

From https://sourceforge.net/projects/smartfilesystem/

```
cvs -z3 -d:pserver:anonymous@a.cvs.sourceforge.net:/cvsroot/smartfilesystem co -P SFS
cvs -z3 -d:pserver:anonymous@a.cvs.sourceforge.net:/cvsroot/smartfilesystem co -P SFSdisplay
```

## cvsclone

Install ``cvsclone`` and its dependency ``flex``:

```bash
sudo yum install flex git-cvs
git clone git@github.com:akavel/cvsclone.git
cd cvsclone
make all
```

## Set paths

Set the following absolute paths:

```bash
# the CVS repository to convert
CVSROOT=:pserver:anonymous@a.cvs.sourceforge.net:/cvsroot/smartfilesystem

# the CVS module to convert
CVSMODULE=SFS 

# the Git branch to create
GITBRANCH=main

# source directory for storing initial local copy of CVS repo
SRCDIR=SFS-src

# destination directory for module specific CVS repo
DESTDIR=SFS-dest

# destination directory for module specific Git repo
GITDIR=smartfilesystem-src
```

## Function for sync

The following function syncronizes out modules from the cloned CVS repo, and must be sourced:

```bash
# Function for syncing file out of repository
synccvs() {
  # Make sure parent directory works, otherwise rsync fails
  mkdir -p `dirname ${DESTDIR}/${1}`
  # Firstly if directory, rsync dir
  if test -d ${SRCDIR}/${1}; then
    rsync -avz ${SRCDIR}/${1}/ ${DESTDIR}/${1}
    return
  fi
  # Next, if file not in attic, rsync that
  if test -e ${SRCDIR}/${1},v; then
    rsync -avz ${SRCDIR}/${1},v ${DESTDIR}/${1},v
    return
  fi
  # Finally, check if file in attic, then rsync that
  if test -e `dirname ${SRCDIR}/${1}`/Attic/`basename ${SRCDIR}/${1}`,v; then
    mkdir -p `dirname ${DESTDIR}/${1}`/Attic
    rsync -avz `dirname ${SRCDIR}/${1}`/Attic/`basename ${SRCDIR}/${1}`,v \
      `dirname ${DESTDIR}/${1}`/Attic/`basename ${DESTDIR}/${1}`,v
    return
  fi
  echo "Path doesn't exist! ${1}"
  exit 1
}
```

## Clone the CVS repo

The CVS repo must be cloned completely, not just checked out. Then the correct module is synced out with the above function.

```bash
cd ${SRCDIR}
cvsclone -d ${CVSROOT} ${CVSMODULE}
synccvs SFS
cvs -d ${DESTDIR} init
```

## Import to Git

Then we import the CVS module we just synced out into Git:

```bash
CVSPSFILE=`echo ${DESTDIR} | sed 's/\//\#/g'`
rm -Rf ~/.cvsps/${CVSPSFILE}*
git cvsimport -d ${DESTDIR} -C ${GITDIR} -p -z,120 -o ${GITBRANCH} -k ${CVSMODULE}
```

## Clean-up

### Encoding

```
Warning: commit message did not conform to UTF-8.
You may want to amend it after fixing the message, or set the config
variable i18n.commitencoding to the encoding your project uses.
```

### Last step

The most important step:

```
yum remove cvs
```