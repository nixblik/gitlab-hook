echo arg0 = $0
echo arg1 = $1
echo arg2 = $2
echo CI_PROJECT_ID = $CI_PROJECT_ID
echo CI_PROJECT_NAME = $CI_PROJECT_NAME
echo CI_PROJECT_SLUG = $CI_PROJECT_SLUG
echo CI_GITLAB_URL = $CI_GITLAB_URL
echo CI_COMMIT_ID = $CI_COMMIT_ID
echo CI_JOBNAMES = $CI_JOBNAMES
echo CI_JOBIDS = $CI_JOBIDS

id
sleep 5

if [ "$2" = "param" ]; then
  exit 1
fi
