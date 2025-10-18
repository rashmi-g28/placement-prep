 angular.module('feedbackApp', [])
.controller('MainCtrl', ['$http', function($http) {
  const vm = this;
  vm.newFeedback = { user: '', text: '', category: 'product' };
  vm.feedbacks = [];
  vm.summary = '';
  vm.status = '';

  vm.loadFeedbacks = function() {
    $http.get('/api/feedbacks').then(resp => vm.feedbacks = resp.data).catch(e => console.error(e));
  };

  vm.submit = function() {
    if (!vm.newFeedback.text) return;
    vm.status = 'Sending...';
    $http.post('/api/feedback', vm.newFeedback).then(resp => {
      vm.status = 'Feedback saved.';
      vm.newFeedback.text = '';
      vm.loadFeedbacks();
      setTimeout(() => vm.status = '', 2000);
    }).catch(err => {
      vm.status = 'Error saving feedback';
      console.error(err);
    });
  };

  vm.getSummary = function() {
    vm.summary = 'Generating...';
    $http.get('/api/summary').then(resp => vm.summary = resp.data.summary || resp.data).catch(err => {
      vm.summary = 'Error generating summary';
      console.error(err);
    });
  };

  // initial load
  vm.loadFeedbacks();
}]);

